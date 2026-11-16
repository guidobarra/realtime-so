#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>
#include <sys/stat.h>
// Agregadas
#include <pthread.h>

extern void mainLoop(void *phy_fb, int width, int height, int color_bytes);
extern void init_std_io();

// Funciones para obtener el framebuffer
static void *get_embeded_fb_in_display_app(int shared_id);
static void *get_dev_fb(char *path, int &width, int &height, int &color_bytes);

// NOTE: Variables para UART y FIFO
#define UART_DEVICE "/dev/ttyUSB1"
#define FIFO_PATH "/tmp/fifo"

// NOTE: Buffer global para los datos recibidos desde UART
char uart_buffer[100];

// NOTE: Función que gestiona la comunicación UART a FIFO
void uart_to_fifo()
{
	// Abrir UART en modo lectura
	int uart_fd = open(UART_DEVICE, O_RDONLY | O_NOCTTY);
	if (uart_fd == -1)
	{
		perror("Error abriendo UART");
		return;
	}

	// Abrir el named pipe en modo escritura
	int fifo_fd = open(FIFO_PATH, O_WRONLY);
	if (fifo_fd == -1)
	{
		perror("Error abriendo named pipe");
		close(uart_fd);
		return;
	}

	// Bucle para leer UART y escribir en el named pipe
	while (1)
	{
		memset(uart_buffer, 0, sizeof(uart_buffer)); // limpia el buffer antes de leer
		int bytes_read = read(uart_fd, uart_buffer, sizeof(uart_buffer) - 1);
		if (bytes_read > 0)
		{
			uart_buffer[bytes_read] = '\0';			 // Asegurar terminación de string
			write(fifo_fd, uart_buffer, bytes_read); // Escribir en el named pipe
		}
		else if (bytes_read == 0)
		{
			// No hay datos, no hacemos nada
			usleep(1000); // Evitar consumir demasiados recursos
		}
		else
		{
			perror("Error leyendo desde UART");
		}
		usleep(100000);
	}

	close(uart_fd);
	close(fifo_fd);
}

// NOTE: Función de hilo para UART a FIFO
void *uart_to_fifo_thread(void *arg)
{
	uart_to_fifo(); // Llamada a la función uart_to_fifo que ya definimos
	return NULL;
}

// Funciones para obtener el framebuffer
static void *get_embeded_fb_in_display_app(int shared_id)
{
	void *ret = NULL;
	while (ret == NULL)
	{
		int shmid = shmget(shared_id, 0, SHM_R | SHM_W);
		if (shmid >= 0)
		{
			if (ret = (unsigned char *)shmat(shmid, 0, 0))
			{
				break;
			}
			perror("shmat failed! run display app first.\n");
		}
		else
		{
			perror("shmget failed! run display app first\n");
		}
		sleep(1);
	}
	return ret;
}

static void *get_dev_fb(char *path, int &width, int &height, int &color_bytes)
{
	int fd = open(path, O_RDWR);
	if (0 > fd)
	{
		perror("open fb failed!\n");
		_exit(-1);
	}

	struct fb_var_screeninfo vinfo;
	if (0 > ioctl(fd, FBIOGET_VSCREENINFO, &vinfo))
	{
		perror("get fb info failed!\n");
		_exit(-1);
	}

	width = vinfo.xres;
	height = vinfo.yres;
	color_bytes = vinfo.bits_per_pixel / 8;
	if (width & 0x3)
	{
		printf("Warning: vinfo.xres should be divided by 4!\nChange your display resolution to meet the rule.\n");
	}
	printf("vinfo.xres=%d\n", vinfo.xres);
	printf("vinfo.yres=%d\n", vinfo.yres);
	printf("vinfo.bits_per_pixel=%d\n", vinfo.bits_per_pixel);

	void *fbp = mmap(0, (vinfo.xres * vinfo.yres * color_bytes), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbp == MAP_FAILED)
	{
		perror("mmap fb failed!\n");
		_exit(-1);
	}
	memset(fbp, 0, (vinfo.xres * vinfo.yres * color_bytes));
	return fbp;
}
static const char *s_welcome =
	"--------------------------- Help ---------------------------\n"
	"Correr usando xWindow: \n"
	"sudo ./xWindow 680 512 | sudo ./programa shared-fb\n\n"

	"Run on ARM Linux:\n"
	".sudo /programa /dev/fb0\n"
	"------------------------------------------------------------\n";

enum FRAMEBUFFER_MODE
{
	FB_NULL_MODE,
	FB_APP_MODE,
	FB_DEV_MODE
};
int main(int argc, char **argv)
{
	printf("%s\n", s_welcome);
	int color_bytes = 2;
	int screen_width = 512;
	int screen_height = 512;

	FRAMEBUFFER_MODE fb_mode = FB_NULL_MODE;
	char *fb_dev_path = NULL;
	int share_id = 1; // should be same with display app.

	if (argc == 2)
	{
		char *argument = argv[1];
		if (strcmp(argument, "shared-fb") == 0)
		{
			fb_mode = FB_APP_MODE;
		}
		else if (strncmp(argument, "/dev", 4) == 0)
		{
			fb_dev_path = argument;
			fb_mode = FB_DEV_MODE;
		}
		else
		{
			printf("Invalid argument, should be: shared-fb, or framebuffer path\n");
			return -1;
		}
	}

	void *phy_fb = NULL;
	switch (fb_mode)
	{
	case FB_APP_MODE:
		phy_fb = get_embeded_fb_in_display_app(share_id);
		break;
	case FB_DEV_MODE:
		phy_fb = get_dev_fb(fb_dev_path, screen_width, screen_height, color_bytes);
		break;
	default:
		phy_fb = calloc(screen_width * screen_height, color_bytes);
		break;
	}

	// NOTE: Crear un hilo para la función uart_to_fifo
	pthread_t uart_thread;
	if (pthread_create(&uart_thread, NULL, uart_to_fifo_thread, NULL) != 0)
	{
		perror("Error creando el hilo UART");
		return -1;
	}

	mainLoop(phy_fb, screen_width, screen_height, color_bytes);

	// NOTE: Esperar a que el hilo termine (aunque uart_to_fifo() corre en un bucle infinito)
	pthread_join(uart_thread, NULL);

	return 0;
}

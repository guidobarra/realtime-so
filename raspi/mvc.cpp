#include <stdio.h>
#define GUILITE_ON
#include "GuiLite.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <cmath>
#include <cmath>

// Declaramos el tamaño de la pantalla
#define UI_WIDTH 512
#define UI_HEIGHT 512

// Cada elemento grafico necesita un ID
// Declaramos una lista, el primer elemento es 1
// y el resto va a tener un ID incremental (2,3,4...)
enum WND_ID
{
	ID_ROOT = 1,
	ID_LABEL_1,
	ID_LABEL_2,
	ID_LABEL_3,
	ID_BUTTON,
	ID_SPIN_BOX,
	ID_SPIN_BOX_2,
	ID_LIST_BOX,
	ID_EDIT_1
};
char tx_buff1[15] = {0};
char tx_buff2[15] = {0};
char tx_buff3[15] = {0};

// Cada elemento grafico necesita un tipo y variable.
static c_label label_1, label_2, label_3;
static c_button button_1;
static c_spin_box spin_box_1, spin_box_2;
static c_list_box list_box_1;
static c_edit edit_1;

static c_display *pantalla;

WND_TREE arbol_elementos[] =
	{

		// Primera fila: Pulsos [edit] [enviar]
		{&label_1, ID_LABEL_1, "Amplitud:", 10, 10, 120, 50}, // x,y,w,h
															  //{&edit_1, ID_EDIT_1, "34", 120, 10, 100, 50},
		{&spin_box_2, ID_SPIN_BOX_2, "spinBox", 120, 10, 100, 50},
		{&button_1, ID_BUTTON, "Enviar", 230, 10, 100, 50},

		// Segunda fila: Duty [spinbox]
		{&label_2, ID_LABEL_2, "Frecuencia:", 10, 70, 145, 50},
		{&spin_box_1, ID_SPIN_BOX, "spinBox", 145, 70, 100, 50},

		// Tercera fila: Opciones [lista]
		{&label_3, ID_LABEL_3, "Tipo:", 10, 140, 100, 50},
		{&list_box_1, ID_LIST_BOX, "listBox", 100, 140, 180, 50},

		// Elemento para indicar el fin del arbol
		{NULL, 0, 0, 0, 0, 0, 0}};

// Creamos la clase MVC que hereda de c_wnd donde esta definido
// todo lo necesario para controlar la GUI
class MVC : public c_wnd
{
	float wave_frequency = 100.0f; // Frecuencia inicial de la onda
	float wave_amplitude = 1.0f;   // Amplitud inicial de la onda
	int wave_type = 0;
	// Esta funcion se llama cuando se inicializa la GUI
	virtual void on_init_children()
	{
		c_spin_box *spin_box_amp = (c_spin_box *)get_wnd_ptr(ID_SPIN_BOX_2); // Segundo spinbox
		spin_box_amp->set_on_change((WND_CALLBACK)&MVC::on_amplitude_change);
		spin_box_amp->set_max_min(3, 1); // Amplitud entre 1 y 3
		spin_box_amp->set_value((int)wave_amplitude);

		// Definimos callback (interrupcion) para el boton de enviar
		c_button *button = (c_button *)get_wnd_ptr(ID_BUTTON);
		button->set_on_click((WND_CALLBACK)&MVC::on_button_clicked);

		// Definimos callback (interrupcion) para el spinbox y lo
		// inicializamos con los valores maximos y minimos
		c_spin_box *spin_box_freq = (c_spin_box *)get_wnd_ptr(ID_SPIN_BOX);
		spin_box_freq->set_on_change((WND_CALLBACK)&MVC::on_frequency_change);
		spin_box_freq->set_max_min(3000, 100); // Frecuencia entre 100 y 3000 Hz
		spin_box_freq->set_value((int)wave_frequency);

		// Definimos callback (interrupcion) para la lista de seleccion
		c_list_box *list_box = (c_list_box *)get_wnd_ptr(ID_LIST_BOX);
		list_box->set_on_change((WND_CALLBACK)&MVC::on_listbox_confirm);
		list_box->clear_item();
		list_box->add_item((char *)"Senoidal");
		list_box->add_item((char *)"Cuadrada");
		list_box->add_item((char *)"Diente Sierra");
		list_box->add_item((char *)"Triangular");
		list_box->select_item(wave_type);
	}

	// Esta función dibuja los elementos gráficos en pantalla
	virtual void on_paint(void)
	{
		m_surface->fill_rect(0, 0, UI_WIDTH, UI_HEIGHT, GL_RGB(0, 0, 0), Z_ORDER_LEVEL_0); // Limpia fondo
		// Dibujamos un recuadro en los bordes usando width y height
		m_surface->draw_rect(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, GL_RGB(0, 255, 0), Z_ORDER_LEVEL_0);
		m_surface->draw_rect(2, 2, UI_WIDTH - 3, UI_HEIGHT - 3, GL_RGB(0, 255, 0), Z_ORDER_LEVEL_0);

		// graficar funciones
		draw_wave();
	}

	// Definimos los manejadores de las interrupciones cuando se produce
	// algun evento en los elementos graficos
	void on_button_clicked(int ctrl_id, int param)
	{
		printf("MVC: Boton Apretado\n");

		int uart_fd = open("/dev/ttyUSB1", O_WRONLY | O_NOCTTY);
		if (uart_fd == -1)
		{
			perror("MVC: Error al abrir UART");
			return;
		}

		// Configurar UART en la PC
		struct termios options;
		tcgetattr(uart_fd, &options);
		cfsetispeed(&options, B1152000);
		cfsetospeed(&options, B1152000);
		options.c_cflag = CS8 | CLOCAL | CREAD;
		tcsetattr(uart_fd, TCSANOW, &options);

		// Cadena a enviar
		char mensaje[] = "Hola ESP32\n";

		// buffer para almacenar la cadena
		char buffer[20];

		snprintf(buffer, sizeof(buffer), "%d,%d,%d", (int)wave_frequency, (int)wave_amplitude, wave_type);
		
		int bytes_written = write(uart_fd, buffer, strlen(buffer));
		printf("Datos concatenados:%s\n", buffer);
		if (bytes_written < 0)
		{
			perror("Error al escribir en UART");
			close(uart_fd);
			return;
		}

	}

	void on_frequency_change(int ctrl_id, int value)
	{
		wave_frequency = value; // Actualiza la frecuencia
		printf("Frecuencia actualizada: %.2f Hz\n", wave_frequency);
	}

	void on_amplitude_change(int ctrl_id, int value)
	{
		wave_amplitude = value; // Actualiza la amplitud
		printf("Amplitud actualizada: %.2f\n", wave_amplitude);
		this->show_window();
	}

	void on_listbox_confirm(int ctrl_id, int value)
	{
		wave_type = value; // actualiza el tipo de señal
		printf("Tipo de señal actualizada: %d\n", wave_type);
		this->show_window();
	}

	void draw_wave()
	{
		int wave_center_y = (UI_HEIGHT / 2) + 50;
		const float display_scale = 0.4f;
		const float max_amplitude = 3.0f;
		float normalized_amplitude = wave_amplitude / max_amplitude;
		int scaled_height = (int)(display_scale * normalized_amplitude * UI_HEIGHT / 2);

		const int cycles_to_draw = UI_WIDTH / 128;

		// Variables para interpolación
		int last_y = wave_center_y;

		for (int px = 0; px < UI_WIDTH; ++px)
		{
			float t_cycle = ((float)px / UI_WIDTH) * cycles_to_draw;
			float t_in_1 = fmod(t_cycle, 1.0f);
			float value = 0.0f;

			switch (wave_type)
			{
			case 0: // Senoidal
				value = sinf(2 * M_PI * t_cycle);
				break;
			case 1: // Cuadrada
				value = (t_in_1 < 0.5f) ? 1.0f : -1.0f;
				break;
			case 2: // Diente de sierra
				value = 2.0f * t_in_1 - 1.0f;
				break;
			case 3: // Triangular
				value = (t_in_1 < 0.5f) ? (4.0f * t_in_1 - 1.0f) : (-4.0f * t_in_1 + 3.0f);
				break;
			default:
				value = 0.0f;
			}

			int y = wave_center_y - (int)(value * scaled_height);

			// Rellenar verticalmente entre el punto anterior y el actual
			int y0 = std::min(y, last_y);
			int y1 = std::max(y, last_y);
			for (int py = y0; py <= y1; ++py)
			{
				m_surface->draw_pixel(px, py, GL_RGB(255, 0, 0), Z_ORDER_LEVEL_0);
			}

			last_y = y;
		}
	}
};

// Creamos un objeto (variable) del tipo MVC (clase definida arriba)
static MVC mvc;

// Hace falta definir un tipografia para el programa, copiamos la
// que se usa por defecto
extern const LATTICE_FONT_INFO Consolas_24B;
void load_resource()
{
	c_theme::add_font(FONT_DEFAULT, &Consolas_24B);
	c_theme::add_color(COLOR_WND_FONT, GL_RGB(255, 255, 243));
	c_theme::add_color(COLOR_WND_NORMAL, GL_RGB(59, 75, 94));
	c_theme::add_color(COLOR_WND_PUSHED, GL_RGB(33, 42, 53));
	c_theme::add_color(COLOR_WND_FOCUS, GL_RGB(78, 198, 76));
	c_theme::add_color(COLOR_WND_BORDER, GL_RGB(46, 59, 73));
}

// Loop Principal.
void mainLoop(void *phy_fb, int width, int height, int color_bytes)
{
	load_resource();
	c_surface *s_surface = new c_surface(UI_WIDTH, UI_HEIGHT, color_bytes, Z_ORDER_LEVEL_2);
	pantalla = new c_display(phy_fb, width, height, s_surface);

	mvc.set_surface(s_surface);
	mvc.connect(NULL, ID_ROOT, 0, 0, 0, UI_WIDTH, UI_HEIGHT, arbol_elementos);
	mvc.show_window();

	char buffer[100];
	// Antes de abrir el fifo, hay que crearlo la primera vez con:
	//  sudo mkfifo /tmp/fifo
	//  y luego darle permisos de escritura a todos los usuarios con:
	//  sudo chmod 666 /tmp/fifo
	int fd = open("/tmp/fifo", O_RDONLY);
	if (fd == -1)
	{
		perror("Error abriendo el named pipe");
		exit(EXIT_FAILURE);
	}

	// Read the message from the named pipe

	while (1)
	{
		int rec = read(fd, buffer, sizeof(buffer));
		if (rec > 0)
		{
			buffer[rec] = 0;
			if (strcmp("adelante", buffer) == 0)
			{
				mvc.on_navigate(NAVIGATION_KEY(0));
				printf("moviendo adelante\n");
			}
			if (strcmp("atras", buffer) == 0)
			{
				mvc.on_navigate(NAVIGATION_KEY(1));
				printf("moviendo atras\n");
			}
			if (strcmp("enter", buffer) == 0)
			{
				mvc.on_navigate(NAVIGATION_KEY(2));
				printf("haciendo enter\n");
			}
		}
	}
}
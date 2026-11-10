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


//Declaramos el tamaño de la pantalla
#define UI_WIDTH 512
#define UI_HEIGHT 512

//Cada elemento grafico necesita un ID
//Declaramos una lista, el primer elemento es 1
//y el resto va a tener un ID incremental (2,3,4...)
enum WND_ID
{
	ID_ROOT = 1,
	ID_LABEL_1,
	ID_LABEL_2,
	ID_LABEL_3,
	ID_BUTTON,
	ID_SPIN_BOX,
	ID_LIST_BOX,
	ID_EDIT_1
};

//Cada elemento grafico necesita un tipo y variable.
static c_label label_1,label_2,label_3;
static c_button button_1;
static c_spin_box spin_box_1;
static c_list_box list_box_1;
static c_edit edit_1;

//La pantalla en si se representa con una variable tipo c_display
static c_display* pantalla;

WND_TREE arbol_elementos[] =
{

	//Primera fila: Pulsos [edit] [enviar]
	{ &label_1,	ID_LABEL_1,	"Pulsos:",	10, 10, 100, 50},
	{ &edit_1,	ID_EDIT_1,	"34",	100, 10, 100, 50},
	{ &button_1,	ID_BUTTON,	"Enviar",	230, 10, 100, 50},
	
	//Segunda fila: Duty [spinbox]
	{ &label_2,	ID_LABEL_2,	"Duty:",	10, 70, 100, 50},
	{ &spin_box_1,	ID_SPIN_BOX,"spinBox",	100, 70, 100, 50},
	
	//Tercera fila: Opciones [lista]
	{ &label_3,	ID_LABEL_3,	"Tipo:",	10, 140, 100, 50},
	{ &list_box_1,	ID_LIST_BOX,"listBox",	100, 140, 180, 50},
		
	//Elemento para indicar el fin del arbol
	{NULL, 0 , 0, 0, 0, 0, 0}
};

//Creamos la clase MVC que hereda de c_wnd donde esta definido
//todo lo necesario para controlar la GUI
class MVC : public c_wnd
{
	//Esta funcion se llama cuando se inicializa la GUI
	virtual void on_init_children() 
	{
		//Definimos tipo de edicion para los pulsos
		c_edit* edit = (c_edit*)get_wnd_ptr(ID_EDIT_1);
		edit->set_keyboard_style(STYLE_NUM_BOARD);
		
		//Definimos callback (interrupcion) para el boton de enviar
		c_button* button = (c_button*)get_wnd_ptr(ID_BUTTON);
		button->set_on_click((WND_CALLBACK)&MVC::on_button_clicked);
		
		//Definimos callback (interrupcion) para el spinbox y lo
		//inicializamos con los valores maximos y minimos
		c_spin_box  *spin_box = (c_spin_box*)get_wnd_ptr(ID_SPIN_BOX);
		spin_box->set_on_change((WND_CALLBACK)&MVC::on_spinbox_change);
		spin_box->set_max_min(100, 1);
		spin_box->set_step(1);
		spin_box->set_value(50);
		
		//Definimos callback (interrupcion) para la lista de seleccion
		c_list_box  *list_box = (c_list_box*)get_wnd_ptr(ID_LIST_BOX);
		list_box->set_on_change((WND_CALLBACK)&MVC::on_listbox_confirm);
		list_box->clear_item();
		list_box->add_item((char*)"Cuadrada");
		list_box->add_item((char*)"Triangular");
		list_box->add_item((char*)"Diente Sierra");
		list_box->select_item(0);
		
	}
	virtual void on_paint(void) 
	{
		//Dibujamos un recuadro en los bordes usando width y height
		m_surface->draw_rect(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, GL_RGB(0, 255, 0), Z_ORDER_LEVEL_0);
		m_surface->draw_rect(2, 2, UI_WIDTH - 3, UI_HEIGHT - 3, GL_RGB(0, 255, 0), Z_ORDER_LEVEL_0);
    }
    
    //Definimos los manejadores de las interrupciones cuando se produce
    //algun evento en los elementos graficos
    void on_button_clicked(int ctrl_id, int param)
	{
		printf("Boton Apretado\n");
	}
		void on_spinbox_change(int ctrl_id, int value)
	{
		printf("Spinbox cambia a : %d\n",value);
	}
	void on_listbox_confirm(int ctrl_id, int value)
	{
		printf( "Seleccionado: %d", value);
	}

};

//Creamos un objeto (variable) del tipo MVC (clase definida arriba)
static MVC mvc;

//Hace falta definir un tipografia para el programa, copiamos la 
//que se usa por defecto
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

//Loop Principal.
void mainLoop(void* phy_fb, int width, int height, int color_bytes){
	load_resource();
	c_surface* s_surface = new c_surface(UI_WIDTH, UI_HEIGHT, color_bytes, Z_ORDER_LEVEL_2);
	pantalla = new c_display(phy_fb, width, height, s_surface);

	mvc.set_surface(s_surface);
	mvc.connect(NULL, ID_ROOT, 0, 0, 0, UI_WIDTH, UI_HEIGHT, arbol_elementos);
	mvc.show_window();
	
	char buffer[100];
	//Antes de abrir el fifo, hay que crearlo la primera vez con:
	// sudo mkfifo /tmp/fifo
	// y luego darle permisos de escritura a todos los usuarios con:
	// sudo chmod 666 /tmp/fifo
	int fd = open("/tmp/fifo", O_RDONLY);
   if (fd == -1) {
      perror("Error abriendo el named pipe");
      exit(EXIT_FAILURE);
   }

   // Read the message from the named pipe
   
	while(1)
	{
		int rec = read(fd, buffer, sizeof(buffer));
		if (rec>0){
			buffer[rec]=0;
				if (strcmp("adelante",buffer)==0){
					mvc.on_navigate(NAVIGATION_KEY(0));
					printf("moviendo adelante\n");
				}
				if (strcmp("atras",buffer)==0){
					mvc.on_navigate(NAVIGATION_KEY(1));
					printf("moviendo atras\n");
				}
				if (strcmp("enter",buffer)==0){
					mvc.on_navigate(NAVIGATION_KEY(2));
					printf("haciendo enter\n");
				}
		}
		
	}
}




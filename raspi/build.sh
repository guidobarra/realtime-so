g++ Consolas_24B.cpp main.cpp mvc.cpp -o Tablero -lpthread

sudo mkfifo /tmp/fifo
sudo chmod 666 /tmp/fifo

echo -n atras > /tmp/fifo
echo -n enter > /tmp/fifo

sudo ./xWindow 512 512 | sudo ./Tablero shared-fb
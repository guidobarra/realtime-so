g++ Consolas_24B.cpp main.cpp mvc.cpp -o Tablero -lpthread

sudo mkfifo /tmp/fifo
sudo chmod 666 /tmp/fifo

echo -n atras > /tmp/fifo
echo -n enter > /tmp/fifo

sudo ./xWindow 512 512 | sudo ./Tablero shared-fb

sudo chmod 777 /dev/ttyUSB0

stty -F /dev/ttyUSB0 115200 raw -echo

cat /dev/ttyUSB0


export LD_LIBRARY_PATH=/usr/local/lib

./Tablero /dev/fb0


detecte-esp32:
	sudo modprobe ftdi_sio
	echo 0403 6001 | sudo tee /sys/bus/usb-serial/drivers/ftdi_sio/new_id
sudo docker build -t my-ubuntu-image .
sudo docker run -itd -v "$PWD/xv6:/home/s081/project" my-ubuntu-image


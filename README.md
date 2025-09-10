for building docker for Pi5

docker build -f docker/Dockerfile -t picowcar-pi5 --build-arg ENV=pi5 .

To build docker for dev env, 
docker build -f docker/Dockerfile -t picowcar-dev --build-arg ENV=dev .


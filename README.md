for building docker for Pi5

docker build -f docker/Dockerfile -t picowcar-pi5 --build-arg ENV=pi5 .

To build docker for dev env, 
docker build -f docker/Dockerfile -t picowcar-dev --build-arg ENV=dev .


After building your Docker container for development, you can use it as follows:

### **1. Run the Container Interactively**
```bash
docker run -it --rm \
  --name picowcar-dev \
  -v $(pwd)/pc-src:/app/pc-src \
  -v $(pwd)/config:/app/config \
  -v $(pwd)/logs:/app/logs \
  picowcar-dev
```
- `-it` gives you an interactive shell.
- `--rm` removes the container after exit.
- `-v ...` mounts your source/config/logs for live editing.
- `picowcar-dev` is the image name you built.

### **2. Run Your Application**
If your Dockerfile’s `CMD` is set to run your app, the container will start it automatically.  
Otherwise, you can run commands inside the container:

```bash
docker exec -it picowcar-dev /bin/bash
# Then run your Python scripts or tests
```

### **3. Use Docker Compose (Recommended for Dev)**
If you have a `docker-compose.dev.yml`, start your dev environment with:
```bash
docker-compose -f docker/docker-compose.dev.yml up
```
This will handle volumes, ports, and environment variables for you.

---

**Summary:**  
- Use `docker run` or `docker-compose` to start your dev container.
- Mount your code as volumes for live development.
- Use `docker exec` for interactive work inside the running container.
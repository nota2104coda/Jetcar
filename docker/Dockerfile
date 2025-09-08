# Use Python 3.11 on Linux (works on both Windows Docker and Pi 5)
FROM python:3.11-slim

# Install system packages needed for Pi 5 hardware and sensors
RUN apt-get update && apt-get install -y \
    # I2C tools for VL53L5X lidar
    i2c-tools \
    # Camera support libraries
    libcamera-dev \
    python3-libcamera \
    # GPIO access
    libgpiod2 \
    # Network tools for Pi Pico W communication
    curl \
    wget \
    # Git for potential updates
    git \
    # Clean up to keep image small
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy Python requirements and install packages
COPY docker/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application code
COPY src/ ./src/
COPY config/ ./config/

# Set Python path
ENV PYTHONPATH=/app

# Create non-root user for security (optional but recommended)
RUN useradd -m -u 1000 robot && chown -R robot:robot /app
USER robot

# Default command to run the robot controller
CMD ["python", "pc-src/main.py"]
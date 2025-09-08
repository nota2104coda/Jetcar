#!/bin/bash
# Deployment script for PicoWCar

set -e

ENVIRONMENT=${1:-"development"}

if [ "$ENVIRONMENT" = "pi5" ]; then
    echo "Deploying to Raspberry Pi 5..."
    
    # Pull latest code
    git pull origin main
    
    # Build and deploy robot controller
    docker-compose -f docker/docker-compose.pi5.yml down
    docker-compose -f docker/docker-compose.pi5.yml build --no-cache
    docker-compose -f docker/docker-compose.pi5.yml up -d
    
    echo "Pi 5 deployment complete!"
    echo "Check status with: docker-compose -f docker/docker-compose.pi5.yml logs -f"
    
elif [ "$ENVIRONMENT" = "development" ]; then
    echo "Starting development environment..."
    
    # Start all development services
    docker-compose -f docker/docker-compose.dev.yml up -d
    
    echo "Development environment started!"
    echo "Robot controller: http://localhost:8080"
    echo "Arduino compilation available via: docker-compose -f docker/docker-compose.dev.yml exec arduino-dev bash"
    
else
    echo "Usage: $0 [development|pi5]"
    exit 1
fi
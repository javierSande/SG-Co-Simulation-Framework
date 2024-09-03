# Use a base image with the necessary dependencies
FROM gcc:latest

# Set the working directory inside the container
WORKDIR /app

# Install Java
RUN apt-get update && \
    apt install -y default-jre

# Install the necessary dependencies
COPY libs/ .

# Copy the source code into the container
COPY rogueSmartMeter/* .

# Compile the libraries
RUN make libs && \
    make modelGenerator && \
    chmod +x startup.sh

# Set the entry point for the container
CMD ["./startup.sh"]
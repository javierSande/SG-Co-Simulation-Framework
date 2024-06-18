# Use a base image with the necessary dependencies
FROM gcc:latest

# Set the working directory inside the container
WORKDIR /app

# Install Java
RUN apt-get update && \
    apt install -y default-jre

# Install the necessary dependencies
RUN git clone -b int16support https://github.com/javierSande/libiec61850.git && \
    cd libiec61850 && \
    make

# Copy the source code into the container
COPY . .

# Compile the libraries
RUN make

# Set the entry point for the container
CMD ["./gooseInjector"]
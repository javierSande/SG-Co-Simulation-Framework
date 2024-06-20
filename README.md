# Smart Grid Co-Simulation Framework

This environment enables us to simulate both the regular operation of the smart grid, and different type of cyber-attacks that exploit cyber-layer de- vices and protocols, allowing us to observe their physical consequences.

## Physical layer - Simulink

The network chosen for our simulation is the IEEE- 14 test grid, a standard test case in power systems re- search and education featuring 14 nodes connected by 20 branches.

### Using other models

## Cyber layer - Emulation

### Smart Meter and Circuit Breaker

Given that for a realistic simulation, each IED needs to function as an independent device with its own IP and MAC address within its respective network, virtualization becomes essential. However, due to the number of IEDs to be emulated and the simplicity of the programs they run, using virtual machines is not scalable. Therefore, the programs responsible for emulating IED logic are encapsulated into Docker images for execution within containers. This approach allows us to deploy a network of IEDs quickly, efficiently, and at scale using [Docker Compose](https://docs.docker.com/compose/). Moreover, by utilizing Docker’s [IPVlan network driver](https://docs.docker.com/network/drivers/ipvlan/) we can integrate these containers into an external network, enabling connection to other systems such as the VM running the RTU or the hardware used for Simulink modeling.

### OpenPLC 61850



![Example PLC runtime](https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/OpenPLC.png)

### SCADA BR

In order to connect OpenPLC to SCADA BR and create an HMI we recommend to follow Thiago Alves' [tutorial](https://www.google.com/url?sa=t&source=web&rct=j&opi=89978449&url=https://www.youtube.com/watch%3Fv%3DKrcL6lhAHKw&ved=2ahUKEwjHyuOLuOqGAxWrBfsDHdlYCQcQwqsBegQIBhAG&usg=AOvVaw2Cztv0H80F2AAP5ekALwCM).

![Example HMI](https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/SCADA.png)

# Smart Grid Co-Simulation Framework

This environment enables us to simulate both the regular operation of the smart grid, and different type of cyber-attacks that exploit cyber-layer de- vices and protocols, allowing us to observe their physical consequences.

## Physical layer - Simulink

The network chosen for our simulation is the IEEE- 14 test grid, a standard test case in power systems re- search and education featuring 14 nodes connected by 20 branches.

<p align="center">
  <img src="https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/Grid.png" width="600">
</p>

### Smart Meter and Circuit Breaker

<p align="center">
  <img src="https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/SM.png" width="400">
</p>
<p align="center">
  <img src="https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/CB.png" width="400">
</p>

### Using other physical models

This framework enables co-simulations with other physical models in MATLAB/Simulink. To achieve this, you need to replace the meters and circuit breakers in the target model with the subsystems available in [IEDs/Simulink](https://github.com/javierSande/SG-Co-Simulation-Framework/tree/main/IEDs/Simulink).

Once all the smart meters and circuit breakers are in place, you must configure the UDP ports used to send and receive data to and from the external program responsible for emulating the logical and communication aspects of the corresponding IED.

## Cyber layer - Emulation
<p align="center">
  <img src="https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/Communications.png" width="400">
</p>

### Smart Meter and Circuit Breaker

Given that for a realistic simulation, each IED needs to function as an independent device with its own IP and MAC address within its respective network, virtualization becomes essential. However, due to the number of IEDs to be emulated and the simplicity of the programs they run, using virtual machines is not scalable. Therefore, the programs responsible for emulating IED logic are encapsulated into Docker images for execution within containers. This approach allows us to deploy a network of IEDs quickly, efficiently, and at scale using [Docker Compose](https://docs.docker.com/compose/). Moreover, by utilizing Docker’s [IPVlan network driver](https://docs.docker.com/network/drivers/ipvlan/) we can integrate these containers into an external network, enabling connection to other systems such as the VM running the RTU or the hardware used for Simulink modeling.

### RTU - OpenPLC 61850

In our model, the RTU serves as the intermediary between the IEDs and the SCADA system, responsible for collecting data from each IED and transmitting it to SCADA. It also receives commands from SCADA operators and forwards them to the IEDs operating as circuit breakers. To achieve this, we used the [OpenPLC61850](https://github.com/smartgridadsc/OpenPLC61850) tool, developed from Thiago Alves’s [OpenPLC project](https://github.com/thiagoralves/OpenPLC_v3).

<p align="center">
  <img src="https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/OpenPLC.png" width="600">
</p>

For the [installation](https://github.com/smartgridadsc/OpenPLC61850?tab=readme-ov-file#4-installation) and [configuration](https://github.com/smartgridadsc/OpenPLC61850?tab=readme-ov-file#51-configuration) of the tool, we recommend following the instructions in the  [OpenPLC61850](https://github.com/smartgridadsc/OpenPLC61850). During the configuration, you will need the following files:

* [IEC 61131-3 (ST) PLC program](https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/OpenPLC-program/program.st)
* [IED SCL files](https://github.com/javierSande/SG-Co-Simulation-Framework/tree/main/IEDs/OpenPLC-ICL-files)
* Server SCL file: Since this functionality of OpenPLC is not used but the file is mandatory, you can leave the example file.

Once OpenPLC61850 is configured, you can start it by pressing the Start PLC button on the web interface.

### SCADA - SCADA BR

In order to connect OpenPLC to SCADA BR and create an HMI we recommend to follow Thiago Alves' [tutorial](https://www.google.com/url?sa=t&source=web&rct=j&opi=89978449&url=https://www.youtube.com/watch%3Fv%3DKrcL6lhAHKw&ved=2ahUKEwjHyuOLuOqGAxWrBfsDHdlYCQcQwqsBegQIBhAG&usg=AOvVaw2Cztv0H80F2AAP5ekALwCM).

<p align="center">
  <img src="https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/SCADA.png" width="600">
</p>

## Recomended setup

In our experiments, simulations were executed using a computer running Simulink and virtual machines hosting a [pfSense](https://www.pfsense.org/download/) router, [OpenPLC61850](https://github.com/smartgridadsc/OpenPLC61850), and [SCADA BR](https://github.com/ScadaBR). The execution of containers, running different IEDs, was divided between two Raspberry Pi connected to the computer via a switch.

<p align="center">
  <img src="https://github.com/javierSande/SG-Co-Simulation-Framework/blob/main/images/HW.png" width="400">
</p>

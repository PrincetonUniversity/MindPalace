  <h1 align="center"> MindPalace </h1>
    <p> MindPalace is a full-system framework that developed to study the architectural behavior of large middleware serverless systems. MindPalace contains a simulation infrastructure and a pre-configured virtual machine (VM) to assist FaaS architecture research. The simulation tool leverages QEMU and ChampSim. It extends QEMU to generate detailed instruction and data access traces with corresponding processor states. These traces are passed to a modified ChampSim to simulate the desired architecture. The VM contains a Debian 12 image with preinstalled OpenWhisk. <p>
</p>

### Contacts
This project is developed by [Princeton Parallel Group](https://parallel.princeton.edu/). If you have any questions, please contact Kaifeng Xu at kaifengx@princeton.edu

### Getting Started
```
git clone https://github.com/PrincetonUniversity/MindPalace.git
```

### Build QEMU
```
cd qemu
mkdir build
cd build
../configure --prefix=PATH/to/sysroot --enable-trace-backends=simple --target-list=x86_64-softmmu,x86_64-linux-user --enable-kvm --disable-vnc --disable-gtk --enable-plugins
make
make install
cd ..
```
Build TCG plugins
```
cd tests/plugin/
make
```

### Download VM for MindPalace
We provide a Debian 12 image with OpenWhisk. https://drive.google.com/file/d/11D3t-0LhTVppDN5ui2dKeVuo8Da0iQ31/view?usp=sharing


### Run QEMU to generate traces
First we need to add QEMU binary location "PATH/to/sysroot" to your .bashrc file. 
```
export PATH=/PATH/to/sysroot/bin:$PATH
```
Source .bashrc if you just added this line.
```
source ~/.bashrc
```
Modify <code>scripts/launch_qemu.sh</code> and set parameters to your path. 
Launch QEMU full system emulation.
```
bash scripts/launch_qemu.sh
```
You can deploy FunctionBench by downloading them from https://github.com/ddps-lab/serverless-faas-workbench.git. And deploy the functions using wsk tool from OpenWhisk. For example:
```
./bin/wsk action create chameleon --docker andersonandrei/python3action:chameleon ~/serverless-faas-workbench/openwhisk/cpu-memory/chameleon/function.py -m 512 -t 300000 -i
```
Invoke functions in QEMU to generate traces by OpenWhisk operations. Functionbench is already set up, here is an example:
```
./bin/wsk action invoke chameleon -p num_of_rows 20 -p num_of_cols 20 -p metadata deadbeef  --result -iv
```

### Run ChampSim
Build ChampSim
```
cd champsim
./config.sh <configuration file>
make
cd ..
```
Modify <code>champsim.sh</code> to set paths and configurations.
Launch ChampSim simulation.
```
bash scripts/champsim.sh
```

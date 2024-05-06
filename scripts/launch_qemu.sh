SIMPLE_FILE=/PATH/TO/nokvm-`date +%Y-%m-%d_-_%H-%M-%S`.trace

DISK=/PATH/TO/my_ubuntu22_image_20230418.qcow2

qemu-system-x86_64 \
    -cpu qemu64,+pcid \
    -icount shift=0,align=off \
    -m 16G \
    -drive if=virtio,file=${DISK},cache=none \
    -loadvm finish_deploy_openwhisk_marker_chameleon \
    -trace events=`pwd`/events,file=${SIMPLE_FILE} \
    -net user,hostfwd=tcp::10022-:22 \
    -net nic \
    -display none \
    -nographic 


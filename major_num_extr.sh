n=$(sudo cat /proc/devices | grep blaster | cut -f1 -d' ')
echo "$n" > major_num.txt

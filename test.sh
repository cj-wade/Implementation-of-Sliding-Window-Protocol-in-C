#!/bin/sh

check_output() {
    cmp .output.$1 .expected_output.$1 2>/dev/null >/dev/null
    if [ $? -eq 0 ]
    then
        echo "PASSED"
        #rm $1 $2
    else
        echo "FAILED"
        echo "Output from datalink binary is in file: .output.$1"
        echo "Expected output is in file: .expected_output.$1"
        echo "Debug output from datalink binary is in file: .debug_output.$1"
    fi
    echo
}

echo "###########################################################################"
echo "NOTE: Guidelines for using this script:"
echo "      - Tests check output written to stdout only."
echo "        If you have debug output in stdout, it may cause test failures."
echo "      - This is not a comprehensive set of test cases."
echo "        It is intended to help you do basic checks on your project."
echo "      - If your program fails any test, it will print the output files"
echo "        generated by your program and the output expected for the test case."
echo "      - The script should take about 20 seconds to complete."
echo "###########################################################################"
echo
echo
echo "Rebuilding datalink..."
make clean; make -j4 datalink
echo "Done rebuilding datalink"
echo


## Test Case 1
echo -n "Test case 1: Sending 1 packet and expecting receiver to print it out: "
(sleep 0.5; echo "msg 0 0 Hello world"; sleep 0.5; echo "exit") | ./datalink -r 1 -s 1 > .output.1 2> .debug_output.1

cat > .expected_output.1 << EOF
<RECV_0>:[Hello world]
EOF

check_output 1


## Test Case 2
echo -n "Test case 2: Sending 10 packets and expecting receiver to print them out in order: "
(sleep 0.5; for i in `seq 1 10`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 2; echo "exit") | ./datalink -r 1 -s 1 > .output.2 2> .debug_output.2

(for i in `seq 1 10`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.2

check_output 2


## Test Case 3
echo -n "Test case 3: Sending 10 packets (with corrupt probability of 40%) and expecting receiver to print them out in order: "
(sleep 2; for i in `seq 1 10`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 8; echo "exit") | ./datalink -c 0.4 -r 1 -s 1 > .output.3 2> .debug_output.3

(for i in `seq 1 10`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.3

check_output 3


## Test Case 4
echo -n "Test case 4: Sending 10 packets (with corrupt probability of 20% and drop probability of 20%) and expecting receiver to print them out in order: "
(sleep 2; for i in `seq 1 10`; do echo "msg 0 0 Packet: $i"; sleep 0.1; done; sleep 8; echo "exit") | ./datalink -d 0.2 -c 0.2 -r 1 -s 1 > .output.4 2> .debug_output.4

(for i in `seq 1 10`; do echo "<RECV_0>:[Packet: $i]"; done) > .expected_output.4

check_output 4


# Test Case 5
echo  -n "Test case 5: Multiple senders and multiple receivers:(with corrupt probability of 20% and drop probability of 20%): "
(sleep 2; for i in 0 1  
 do 
    for j in 0 1 
        do 
            for k in 1 2 3 4 5
                do echo "msg $i $j Packet: $k"
                sleep 0.1
                done
        done      
done  
sleep 5;   
echo "exit") | ./datalink -d 0.2 -c 0.2 -r 2 -s 2 > .output.5 2> .debug_output.5

(sleep 0.5;for i in 0 1  
 do 
    for j in 0 1 
        do 
            for k in 1 2 3 4 5
                do echo "<RECV_$j>:[Packet: $k]"
                sleep 0.1
                done
        done
done )> .expected_output.5

check_output 5

echo "Completed test cases"
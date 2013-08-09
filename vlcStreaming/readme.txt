0) install python:

1) run vlc host:
run_vlc_host.sh

2) run streams:
python vlcstream.py host_ip streaming_port num_of_streams path_to_video
	
	host_ip - your ip (not localhost!)
	streaming_port - port in rtsp address
	num_of_streams - obviously
	path_to_video - if it .mp4 file, then all streams will be broadcast it; 
					if it is dir with .mp4 files, then concrete stream will be broadcast concrete .mp4. for example 10 streams and 7 files means that 1, 2 and 3 file will be broadcasted twice(by 1,2,3,8,9,10 streams), 4,5,6,7 - once

example: python vlcstream.py 192.168.10.21 8092 10 /home/igorp/Downloads/outFashD1.mp4
to test: vlc rtsp://192.168.10.21:8092/test9.sdp

3) to reastart vlc host:
do 'ps -ax | grep vlc'
find out pid of vlc
do 'kill pid'
go to 1)

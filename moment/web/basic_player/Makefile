TARGET = BasicPlayer.swf

.PHONY: all clean

all: $(TARGET)

$(TARGET): BasicPlayer.as
	/opt/flex_4.0.0/bin/mxmlc -headless-server=true -static-link-runtime-shared-libraries=true -target-player=10.0 $^
#	/opt/flex/bin/mxmlc -headless-server=true -static-link-runtime-shared-libraries=true $^
#	/opt/flex/bin/mxmlc -headless-server=true -debug=true $^
#	/opt/flex/bin/mxmlc $^
#	/opt/flex/bin/mxmlc -debug=true $^

clean:
	rm -f $(TARGET)


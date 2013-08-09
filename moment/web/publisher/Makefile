TARGET = Publisher.swf

.PHONY: all clean

all: $(TARGET)

$(TARGET): Publisher.as
	/opt/flex/bin/mxmlc -headless-server=true -static-link-runtime-shared-libraries=true $^

clean:
	rm -f $(TARGET)


TARGET = sbs

CFLAGS = -Wall -O2 

OBJS =  i2cbusses.o sbs.o

.SUFFIXES : .c .o

all : $(TARGET)

$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(TARGET)

#pragma once

// grabbed from https://github.com/Industrial-Shields/arduino-BufferStream

class BaseBufferStream : public Stream
{
  public:
    explicit BaseBufferStream(void *buff, size_t len);

  public:
    virtual size_t write(uint8_t value) { return 0; }
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}

    using Print::write;

  protected:
    uint8_t *buff;
    size_t len;
};

BaseBufferStream::BaseBufferStream(void *_buff, size_t len) : buff((uint8_t*)_buff), len(len) { }

////////////////////////////////////////////////////////////////////////////////////////////////////

class ReadBufferStream : public BaseBufferStream
{
  public:
    explicit ReadBufferStream(void *buff, size_t len);

  public:
    virtual int available();
    virtual int read();
    virtual int peek();
};


ReadBufferStream::ReadBufferStream(void *buff, size_t len) : BaseBufferStream(buff, len) { }

int ReadBufferStream::available() { return len; }

int ReadBufferStream::read()
{
  if (len == 0) {
    return -1;
  }
  --len;
  return *buff++;
}

int ReadBufferStream::peek()
{
  if (len == 0) {
    return -1;
  }
  return *buff;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

class WriteBufferStream : public BaseBufferStream
{
  public:
    explicit WriteBufferStream(void *buff, size_t len);

  public:
    virtual size_t write(uint8_t value);

    using Print::write;
};

WriteBufferStream::WriteBufferStream(void *buff, size_t len) : BaseBufferStream(buff, len) {}

size_t WriteBufferStream::write(uint8_t value)
{
  if (len == 0) {
    return 0;
  }
  *buff++ = value;
  --len;
  return 1;
}



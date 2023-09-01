/// @file amf.cc
/// @brief 
/// @version 0.1
/// @author lq
/// @date 2023/06/04

#include "amf.h"

#include "BufferReader.h"
#include "BufferWriter.h"

int AmfDecoder::Decode(const char *data, int size, int n) {
  int bytes_used = 0;
  while (size > bytes_used) {
    int ret = 0;
    char type = data[bytes_used];
    bytes_used += 1;

    switch (type) {
      case AMF0_NUMBER:
        obj_.type = AMF_NUMBER;
        ret = DecodeNumber(data + bytes_used, size - bytes_used, obj_.amf_number);
        break;

      case AMF0_BOOLEAN:
        obj_.type = AMF_BOOLEAN;
        ret = DecodeBoolean(data + bytes_used, size - bytes_used, obj_.amf_boolean);
        break;

      case AMF0_STRING:
        obj_.type = AMF_STRING;
        ret = DecodeString(data + bytes_used, size - bytes_used, obj_.amf_string);
        break;

      case AMF0_OBJECT:
        ret = DecodeObject(data + bytes_used, size - bytes_used, objs_);
        break;

      case AMF0_OBJECT_END:
        break;

      case AMF0_ECMA_ARRAY:
        ret = DecodeObject(data + bytes_used + 4, size - bytes_used - 4, objs_);
        break;

      case AMF0_NULL:
        break;

      default:
        break;
    }

    if (ret < 0) {
      break;
    }

    bytes_used += ret;
    n--;
    if (n == 0) {
      break;
    }
  }

  return bytes_used;
}

int AmfDecoder::DecodeNumber(const char *data, int size, double &amf_number) {
  if (size < 8) {
    return 0;
  }
  // amf 大端序(网络字节序), 转成小端序(主机字节序)
  char *ci = (char *)data;
  char *co = (char *)&amf_number;
  co[0] = ci[7];
  co[1] = ci[6];
  co[2] = ci[5];
  co[3] = ci[4];
  co[4] = ci[3];
  co[5] = ci[2];
  co[6] = ci[1];
  co[7] = ci[0];

  return 8;
}

int AmfDecoder::DecodeString(const char *data, int size, std::string &amf_string) {
  if (size < 2) {
    return 0;
  }

  int bytes_used = 0;
  int strSize = DecodeInt16(data, size);
  bytes_used += 2;
  if (strSize > (size - bytes_used)) {
    return -1;
  }

  amf_string = std::string(&data[bytes_used], 0, strSize);
  bytes_used += strSize;
  return bytes_used;
}

int AmfDecoder::DecodeObject(const char *data, int size, AmfObjects &amf_objs) {
  amf_objs.clear();
  int bytes_used = 0;
  while (size > 0) {
    int strLen = DecodeInt16(data + bytes_used, size);
    size -= 2;
    if (size < strLen) {
      return bytes_used;
    }

    std::string key(data + bytes_used + 2, 0, strLen);
    size -= strLen;

    AmfDecoder dec;
    int ret = dec.Decode(data + bytes_used + 2 + strLen, size, 1);
    bytes_used += 2 + strLen + ret;
    if (ret <= 1) {
      break;
    }

    amf_objs.emplace(key, dec.GetObject());
  }

  return bytes_used;
}

int AmfDecoder::DecodeBoolean(const char *data, int size, bool &amf_boolean) {
  if (size < 1) {
    return 0;
  }

  amf_boolean = (data[0] != 0);
  return 1;
}

uint16_t AmfDecoder::DecodeInt16(const char *data, int size) {
  uint16_t val = ReadUint16BE((char *)data);
  return val;
}

uint32_t AmfDecoder::DecodeInt24(const char *data, int size) {
  uint32_t val = ReadUint24BE((char *)data);
  return val;
}

uint32_t AmfDecoder::DecodeInt32(const char *data, int size) {
  uint32_t val = ReadUint32BE((char *)data);
  return val;
}

AmfEncoder::AmfEncoder(uint32_t size)
    : data_(new char[size], std::default_delete<char[]>()), size_(size) {}

AmfEncoder::~AmfEncoder() {}

void AmfEncoder::EncodeInt8(int8_t value) {
  if ((size_ - index_) < 1) {
    this->Realloc(size_ + 1024);
  }

  data_.get()[index_++] = value;
}

void AmfEncoder::EncodeInt16(int16_t value) {
  if ((size_ - index_) < 2) {
    this->Realloc(size_ + 1024);
  }

  WriteUint16BE(data_.get() + index_, value);
  index_ += 2;
}

void AmfEncoder::EncodeInt24(int32_t value) {
  if ((size_ - index_) < 3) {
    this->Realloc(size_ + 1024);
  }

  WriteUint24BE(data_.get() + index_, value);
  index_ += 3;
}

void AmfEncoder::EncodeInt32(int32_t value) {
  if ((size_ - index_) < 4) {
    this->Realloc(size_ + 1024);
  }

  WriteUint32BE(data_.get() + index_, value);
  index_ += 4;
}

void AmfEncoder::EncodeString(const char *str, int len, bool isObject) {
  if ((int)(size_ - index_) < (len + 1 + 2 + 2)) {
    this->Realloc(size_ + len + 5);
  }

  if (len < 65536) {
    if (isObject) {
      data_.get()[index_++] = AMF0_STRING;
    }
    EncodeInt16(len);
  } else {
    if (isObject) {
      data_.get()[index_++] = AMF0_LONG_STRING;
    }
    EncodeInt32(len);
  }

  memcpy(data_.get() + index_, str, len);
  index_ += len;
}

void AmfEncoder::EncodeNumber(double value) {
  if ((size_ - index_) < 9) {
    this->Realloc(size_ + 1024);
  }

  data_.get()[index_++] = AMF0_NUMBER;

  char *ci = (char *)&value;
  char *co = data_.get();
  co[index_++] = ci[7];
  co[index_++] = ci[6];
  co[index_++] = ci[5];
  co[index_++] = ci[4];
  co[index_++] = ci[3];
  co[index_++] = ci[2];
  co[index_++] = ci[1];
  co[index_++] = ci[0];
}

void AmfEncoder::EncodeBoolean(int value) {
  if ((size_ - index_) < 2) {
    this->Realloc(size_ + 1024);
  }

  data_.get()[index_++] = AMF0_BOOLEAN;
  data_.get()[index_++] = value ? 0x01 : 0x00;
}

void AmfEncoder::EncodeObjects(AmfObjects &objs) {
  if (objs.size() == 0) {
    EncodeInt8(AMF0_NULL);
    return;
  }

  EncodeInt8(AMF0_OBJECT);

  for (auto iter : objs) {
    EncodeString(iter.first.c_str(), (int)iter.first.size(), false);
    switch (iter.second.type) {
      case AMF_NUMBER:
        EncodeNumber(iter.second.amf_number);
        break;
      case AMF_STRING:
        EncodeString(iter.second.amf_string.c_str(), (int)iter.second.amf_string.size());
        break;
      case AMF_BOOLEAN:
        EncodeBoolean(iter.second.amf_boolean);
        break;
      default:
        break;
    }
  }

  EncodeString("", 0, false);
  EncodeInt8(AMF0_OBJECT_END);
}

void AmfEncoder::EncodeECMA(AmfObjects &objs) {
  EncodeInt8(AMF0_ECMA_ARRAY);
  EncodeInt32(0);

  for (auto iter : objs) {
    EncodeString(iter.first.c_str(), (int)iter.first.size(), false);
    switch (iter.second.type) {
      case AMF_NUMBER:
        EncodeNumber(iter.second.amf_number);
        break;
      case AMF_STRING:
        EncodeString(iter.second.amf_string.c_str(), (int)iter.second.amf_string.size());
        break;
      case AMF_BOOLEAN:
        EncodeBoolean(iter.second.amf_boolean);
        break;
      default:
        break;
    }
  }

  EncodeString("", 0, false);
  EncodeInt8(AMF0_OBJECT_END);
}

void AmfEncoder::Realloc(uint32_t size) {
  if (size <= size_) {
    return;
  }

  std::shared_ptr<char> data(new char[size], std::default_delete<char[]>());
  memcpy(data.get(), data_.get(), index_);
  size_ = size;
  data_ = data;
}

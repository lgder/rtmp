/// @file amf.h
/// @brief Action Message Format 简化封装, 参考 librtmp 使用 C++11 封装
/// @version 0.1
/// @author lq
/// @date 2023/06/04

#ifndef RTMP_SERVER_AMF_H
#define RTMP_SERVER_AMF_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

typedef enum {
  AMF0_NUMBER = 0,  // 注意 amf 的数字类型只能是 double
  AMF0_BOOLEAN,
  AMF0_STRING,
  AMF0_OBJECT,
  AMF0_MOVIECLIP, /* reserved, not used */
  AMF0_NULL,
  AMF0_UNDEFINED,
  AMF0_REFERENCE,
  AMF0_ECMA_ARRAY,  // 数组, 键值对形式存储元素, 类似 JSON
  AMF0_OBJECT_END,
  AMF0_STRICT_ARRAY,
  AMF0_DATE,
  AMF0_LONG_STRING,
  AMF0_UNSUPPORTED,
  AMF0_RECORDSET, /* reserved, not used */
  AMF0_XML_DOC,
  AMF0_TYPED_OBJECT,
  AMF0_AVMPLUS, /* switch to AMF3 */
  AMF0_INVALID = 0xff
} AMF0DataType;

typedef enum {
  AMF3_UNDEFINED = 0,
  AMF3_NULL,
  AMF3_FALSE,
  AMF3_TRUE,
  AMF3_INTEGER,
  AMF3_DOUBLE,
  AMF3_STRING,
  AMF3_XML_DOC,
  AMF3_DATE,
  AMF3_ARRAY,
  AMF3_OBJECT,
  AMF3_XML,
  AMF3_BYTE_ARRAY
} AMF3DataType;

typedef enum {
  AMF_NUMBER,
  AMF_BOOLEAN,
  AMF_STRING,
} AmfObjectType;

struct AmfObject {
  AmfObjectType type;

  std::string amf_string;
  double amf_number;
  bool amf_boolean;

  AmfObject() {}

  AmfObject(std::string str) {
    this->type = AMF_STRING;
    this->amf_string = str;
  }

  AmfObject(double number) {
    this->type = AMF_NUMBER;
    this->amf_number = number;
  }
};

typedef std::unordered_map<std::string, AmfObject> AmfObjects;

class AmfDecoder {
 public:
  /// @brief 通用解码函数, 根据输入数据的类型调用相应的解码函数
  /// 
  /// @param data 二进制数据的数组
  /// @param size data 的大小
  /// @param n 解码次数
  /// @return int 
  int Decode(const char *data, int size, int n = -1);

  void Reset() {
    obj_.amf_string = "";
    obj_.amf_number = 0;
    objs_.clear();
  }

  std::string GetString() const { return obj_.amf_string; }

  double GetNumber() const { return obj_.amf_number; }

  bool HasObject(std::string key) const { return (objs_.find(key) != objs_.end()); }

  AmfObject GetObject(std::string key) { return objs_[key]; }

  AmfObject GetObject() { return obj_; }

  AmfObjects GetObjects() { return objs_; }

 private:
  static int DecodeBoolean(const char *data, int size, bool &amf_boolean);
  static int DecodeNumber(const char *data, int size, double &amf_number);
  static int DecodeString(const char *data, int size, std::string &amf_string);
  static int DecodeObject(const char *data, int size, AmfObjects &amf_objs);
  static uint16_t DecodeInt16(const char *data, int size);
  static uint32_t DecodeInt24(const char *data, int size);
  static uint32_t DecodeInt32(const char *data, int size);

  AmfObject obj_;
  AmfObjects objs_;
};

class AmfEncoder {
 public:
  AmfEncoder(uint32_t size = 1024);
  virtual ~AmfEncoder();

  void Reset() { index_ = 0; }

  std::shared_ptr<char> Data() { return data_; }

  uint32_t Size() const { return index_; }

  void EncodeString(const char *str, int len, bool isObject = true);
  void EncodeNumber(double value);
  void EncodeBoolean(int value);
  void EncodeObjects(AmfObjects &objs);
  void EncodeECMA(AmfObjects &objs);

 private:
  void EncodeInt8(int8_t value);
  void EncodeInt16(int16_t value);
  void EncodeInt24(int32_t value);
  void EncodeInt32(int32_t value);
  void Realloc(uint32_t size);

  std::shared_ptr<char> data_;
  uint32_t size_ = 0;
  uint32_t index_ = 0;
};

#endif  // RTMP_SERVER_AMF_H

/// @file H264File.cc
/// @brief
/// @version 0.1
/// @author lq
/// @date 2023/06/30

#include "H264File.h"

#include <cstdio>
#include <cstring>

H264File::H264File(int buf_size_) : buf_size_(buf_size_), buf_(new char[buf_size_]) {}

H264File::~H264File() { delete[] buf_; }

bool H264File::open(const char *path) {
  file_ = fopen(path, "rb");
  if (file_ == NULL) {
    return false;
  }

  return true;
}

void H264File::Close() {
  if (file_) {
    fclose(file_);
    file_ = NULL;
    count_ = 0;
    bytes_used_ = 0;
  }
}

int H264File::readFrame(char *in_buf, int in_buf_size, bool *is_end_of_frame) {
  if (file_ == NULL) {
    return -1;
  }

  int bytesRead = (int)fread(buf_, 1, buf_size_, file_);
  if (bytesRead == 0) {
    fseek(file_, 0, SEEK_SET);
    count_ = 0;
    bytes_used_ = 0;
    bytesRead = (int)fread(buf_, 1, buf_size_, file_);
    if (bytesRead == 0) {
      this->Close();
      return -1;
    }
  }

  bool bFindStart = false, bFindEnd = false;

  int i = 0, startCode = 3;
  *is_end_of_frame = false;
  // 下面搜索 H264 帧的开始和结束标记, 开始标记是 0x00 0x00 0x01 或 0x00 0x00 0x00 0x01
  for (i = 0; i < bytesRead - 5; i++) {
    if (buf_[i] == 0 && buf_[i + 1] == 0 && buf_[i + 2] == 1) {
      startCode = 3;
    } else if (buf_[i] == 0 && buf_[i + 1] == 0 && buf_[i + 2] == 0 && buf_[i + 3] == 1) {
      startCode = 4;
    } else {
      continue;
    }

    if (((buf_[i + startCode] & 0x1F) == 0x5 || (buf_[i + startCode] & 0x1F) == 0x1) &&
        ((buf_[i + startCode + 1] & 0x80) == 0x80)) {
      bFindStart = true;
      i += 4;
      break;
    }
  }

  // 结束标记是0x00 0x00 0x01后跟0x7, 0x8, 0x6, 或者0x5或0x1并且下一个字节的最高位是1
  for (; i < bytesRead - 5; i++) {
    if (buf_[i] == 0 && buf_[i + 1] == 0 && buf_[i + 2] == 1) {
      startCode = 3;
    } else if (buf_[i] == 0 && buf_[i + 1] == 0 && buf_[i + 2] == 0 && buf_[i + 3] == 1) {
      startCode = 4;
    } else {
      continue;
    }

    if (((buf_[i + startCode] & 0x1F) == 0x7) || ((buf_[i + startCode] & 0x1F) == 0x8) ||
        ((buf_[i + startCode] & 0x1F) == 0x6) ||
        (((buf_[i + startCode] & 0x1F) == 0x5 || (buf_[i + startCode] & 0x1F) == 0x1) &&
         ((buf_[i + startCode + 1] & 0x80) == 0x80))) {
      bFindEnd = true;
      break;
    }
  }

  bool flag = false;
  if (bFindStart && !bFindEnd && count_ > 0) {
    flag = bFindEnd = true;
    i = bytesRead;
    *is_end_of_frame = true;
  }

  if (!bFindStart || !bFindEnd) {
    this->Close();
    return -1;
  }

  int size = (i <= in_buf_size ? i : in_buf_size);
  memcpy(in_buf, buf_, size);

  if (!flag) {
    count_ += 1;
    bytes_used_ += i;
  } else {
    count_ = 0;
    bytes_used_ = 0;
  }

  fseek(file_, bytes_used_, SEEK_SET);
  return size;
}

Nal H264File::findNal(const uint8_t *data, uint32_t size) {
  Nal nal(nullptr, nullptr);

  if (size < 5) {
    return nal;
  }

  nal.second = const_cast<uint8_t *>(data) + (size - 1);

  // uint32_t startCode = 0;
  uint32_t pos = 0;
  uint8_t prefix[3] = {0};
  memcpy(prefix, data, 3);
  size -= 3;
  data += 2;

  while (size--) {
    // 00 00 01 起始码
    if ((prefix[pos % 3] == 0) && (prefix[(pos + 1) % 3] == 0) && (prefix[(pos + 2) % 3] == 1)) {
      if (nal.first == nullptr) {
        nal.first = const_cast<uint8_t *>(data) + 1;
        // startCode = 3;
      } else if (data > nal.first + 3) {
        nal.second = const_cast<uint8_t *>(data) - 3;
        break;
      }
    } else if ((prefix[pos % 3] == 0) && (prefix[(pos + 1) % 3] == 0) &&
               (prefix[(pos + 2) % 3] == 0)) {
      if (*(data + 1) == 0x01) {  // 00 00 00 01 起始码
        if (nal.first == nullptr) {
          if (size >= 1) {
            nal.first = const_cast<uint8_t *>(data) + 2;
          } else {
            break;
          }
          // startCode = 4;
        } else {  // if(startCode == 4)
          nal.second = const_cast<uint8_t *>(data) - 3;
          break;
        }
      }
    }

    prefix[(pos++) % 3] = *(++data);
  }

  if (nal.first == nullptr) nal.second = nullptr;

  return nal;
}

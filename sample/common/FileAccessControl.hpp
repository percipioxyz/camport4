// FileAccessControl.hpp
#ifndef FILE_ACCESS_CONTROL_HPP
#define FILE_ACCESS_CONTROL_HPP

#include <iostream>
#include <streambuf>
#include <vector>
#include <map>
#include "TYApi.h"

namespace percipio {

typedef enum TY_FILE_SEL_LIST :uint32_t
{
    FILE_SEL_DEFAULT0 = 0,   ///< Default user set 0
    FILE_SEL_DEFAULT1 = 1,   ///< Default user set 1
    FILE_SEL_DEFAULT2 = 2,   ///< Default user set 2
    FILE_SEL_DEFAULT3 = 3,   ///< Default user set 3
    FILE_SEL_DEFAULT4 = 4,   ///< Default user set 4
    FILE_SEL_DEFAULT5 = 5,   ///< Default user set 5
    FILE_SEL_DEFAULT6 = 6,   ///< Default user set 6
    FILE_SEL_DEFAULT7 = 7,   ///< Default user set 7

    FILE_SEL_USER_SET0 = 8,  ///< Custom user set 0
    FILE_SEL_USER_SET1 = 9,  ///< Custom user set 1
    FILE_SEL_USER_SET2 = 10, ///< Custom user set 2
    FILE_SEL_USER_SET3 = 11, ///< Custom user set 3
    FILE_SEL_USER_SET4 = 12, ///< Custom user set 4
    FILE_SEL_USER_SET5 = 13, ///< Custom user set 5
    FILE_SEL_USER_SET6 = 14, ///< Custom user set 6
    FILE_SEL_USER_SET7 = 15, ///< Custom user set 7

    FILE_SEL_CUSTOM0 = 17,   ///< Custom file 0
    FILE_SEL_CUSTOM1 = 18,   ///< Custom file 1
    FILE_SEL_CUSTOM2 = 19,   ///< Custom file 2
    FILE_SEL_CUSTOM3 = 20,   ///< Custom file 3
    FILE_SEL_CUSTOM4 = 21,   ///< Custom file 4
    FILE_SEL_CUSTOM5 = 22,   ///< Custom file 5
    FILE_SEL_CUSTOM6 = 23,   ///< Custom file 6
    FILE_SEL_CUSTOM7 = 24,   ///< Custom file 7
} TY_FILE_SEL_LIST;
typedef uint32_t TY_FILE_SEL;

inline const std::map<uint32_t, std::string>& getFileMaps() {
    static const std::map<uint32_t, std::string> fileMaps = {
        {FILE_SEL_DEFAULT0, "Default0"},
        {FILE_SEL_DEFAULT1, "Default1"},
        {FILE_SEL_DEFAULT2, "Default2"},
        {FILE_SEL_DEFAULT3, "Default3"},
        {FILE_SEL_DEFAULT4, "Default4"},
        {FILE_SEL_DEFAULT5, "Default5"},
        {FILE_SEL_DEFAULT6, "Default6"},
        {FILE_SEL_DEFAULT7, "Default7"},

        {FILE_SEL_USER_SET0, "UserSet0"},
        {FILE_SEL_USER_SET1, "UserSet1"},
        {FILE_SEL_USER_SET2, "UserSet2"},
        {FILE_SEL_USER_SET3, "UserSet3"},
        {FILE_SEL_USER_SET4, "UserSet4"},
        {FILE_SEL_USER_SET5, "UserSet5"},
        {FILE_SEL_USER_SET6, "UserSet6"},
        {FILE_SEL_USER_SET7, "UserSet7"},

        {FILE_SEL_CUSTOM0, "CustomFile0"},
        {FILE_SEL_CUSTOM1, "CustomFile1"},
        {FILE_SEL_CUSTOM2, "CustomFile2"},
        {FILE_SEL_CUSTOM3, "CustomFile3"},
        {FILE_SEL_CUSTOM4, "CustomFile4"},
        {FILE_SEL_CUSTOM5, "CustomFile5"},
        {FILE_SEL_CUSTOM6, "CustomFile6"},
        {FILE_SEL_CUSTOM7, "CustomFile7"},
    };
    return fileMaps;
}

typedef enum TY_FILE_OP_SEL_LIST :uint32_t
{
    FILE_OP_SEL_OPEN  ,
    FILE_OP_SEL_CLOSE ,
    FILE_OP_SEL_READ  ,
    FILE_OP_SEL_WRITE ,
    FILE_OP_SEL_DELETE,
} TY_FILE_OP_SEL_LIST;
typedef uint32_t TY_FILE_OP_SEL;

typedef enum TY_FILE_OPEN_MODE_LIST :uint32_t
{
    FILE_OPEN_MODE_READ      ,
    FILE_OPEN_MODE_WRITE     ,
    FILE_OPEN_MODE_READWRITE ,
} TY_FILE_OPEN_MODE_LIST;
typedef uint32_t TY_FILE_OPEN_MODE;

typedef enum TY_FILE_OP_STATUS_LIST :uint32_t
{
    FILE_OP_STATUS_SUCC ,
    FILE_OP_STATUS_FAIL ,
} TY_FILE_OP_STATUS_LIST;
typedef uint32_t TY_FILE_OP_STATUS;


class FileAccessControlBuf : public std::streambuf {
private:
    TY_DEV_HANDLE m_hDevice;
    std::string m_fileSelector;
    std::vector<char> m_buffer;
    size_t m_bufferSize;
    long int m_currentPosition;
    bool m_isOpen;
    
public:
    explicit FileAccessControlBuf(TY_DEV_HANDLE hDevice, size_t bufferSize = 4096);
    ~FileAccessControlBuf();
    
    // File API
    bool open(const char *file_name, std::ios::openmode mode);
    bool close();
    bool isOpen() const { return m_isOpen; }
    
    // set file Selector
    void setFileSelector(const char *fileSel) { m_fileSelector = fileSel; }
    const std::string& getFileSelector() const { return m_fileSelector; }
    
protected:
    // override streambuf func
    virtual int_type underflow() override;
    virtual int_type overflow(int_type ch = traits_type::eof()) override;
    virtual int sync() override;
    virtual pos_type seekoff(off_type off, std::ios_base::seekdir way,
                           std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
    virtual pos_type seekpos(pos_type pos, 
                           std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override;
    
private:
    size_t readFromFile(void* buf, size_t count);
    size_t writeToFile(const void* buf, size_t count);
    bool openFile(TY_FILE_OPEN_MODE openMode);
    bool closeFile();
    bool deleteFile();
    TY_STATUS seekTofile();
    size_t getFileSize();
};

class FileAccessControl : public std::iostream {
private:
    FileAccessControlBuf m_buf;
    
public:
    explicit FileAccessControl(TY_DEV_HANDLE hDevice, size_t bufferSize = 4096);
    
    // File API
    bool open(const char *file_name, std::ios::openmode mode);
    bool close();
    bool isOpen() const;
    
    // set file Selector
    void setFileSelector(const char * fileSel);
    const std::string& getFileSelector() const;
    
    // get buffer
    FileAccessControlBuf* rdbuf() const;
};

} // namespace percipio

#endif // FILE_ACCESS_CONTROL_HPP

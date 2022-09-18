//#define T_DIR  1   // Directory
//#define T_FILE 2   // File
//#define T_DEV  3   // Device

struct iostat {
    int readcount;
    // total number of times read() is invoked by the current running process
    // (including failed calls)

    int writecount;
    // total number of times write() is invoked by the current running process
    //  (including failed calls)
};

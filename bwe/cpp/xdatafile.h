
#ifndef  _XDATAFILE_H
#define  _XDATAFILE_H



#ifdef __cplusplus
extern "C" {
#endif


typedef struct xdata_item{
    int row;
    double value;
}xdata_item;

typedef struct xvector{
    char name[128];
    int num_rows;
    xdata_item * data; 
}xvector;

typedef struct xvector_array{
    xvector * vectors;
    int num_vectors;
}xvector_array;

xvector_array * load_txt(const char * filename);
void xvector_array_free(xvector_array * arr);


#ifdef __cplusplus
}
#endif

#endif
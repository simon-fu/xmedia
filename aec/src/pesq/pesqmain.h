//
//  pesqmain.h
//  app_audio
//
//  Created by simon on 2018/6/8.
//  Copyright © 2018年 easemob. All rights reserved.
//

#ifndef pesqmain_h
#define pesqmain_h

#ifdef __cplusplus
extern "C"{
#endif
    
int pesq_wav(int sampleRate, const char * ref_filename, const char * deg_filename, float * p_pesq_mos, float * p_mapped_mos);
    
#ifdef __cplusplus
}
#endif
        
#endif /* pesqmain_h */

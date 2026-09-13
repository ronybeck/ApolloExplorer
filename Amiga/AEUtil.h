/*
 * AEUtil.h
 *
 *  Created on: May 16, 2021
 *      Author: rony
 */

#ifndef AMIGA_AEUTIL_H_
#define AMIGA_AEUTIL_H_

#if DBGOUT == 1
#include <stdio.h>
#define dbglog( format, ... ) printf( format, ##__VA_ARGS__ );
#else
#define dbglog( format, ... )
#endif

//The following BPCL macros are from the AROS project.
#define AROS_BSTR_ADDR(s)        (((STRPTR)BADDR(s))+1)
#define AROS_BSTR_strlen(s)      (AROS_BSTR_ADDR(s)[-1])
#define AROS_BSTR_setstrlen(s,l) do { \
    STRPTR _s = AROS_BSTR_ADDR(s); \
    _s[-1] = l; \
    _s[l]=0; \
} while(0)

#endif /* AMIGA_AEUTIL_H_ */

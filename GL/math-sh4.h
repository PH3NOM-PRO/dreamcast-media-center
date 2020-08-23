#ifndef MATH_SH4
#define MATH_SH4

inline float SHFMAC( float a, float b, float c )
{
     register float __FR0 __asm__("fr0") = a; 
     register float __FR1 __asm__("fr1") = b; 
     register float __FR2 __asm__("fr2") = c;      
     
     __asm__ __volatile__( 
        "fmac   fr0, fr1, fr2\n"
        : "=f" (__FR0), "=f" (__FR1), "=f" (__FR2)
        : "0" (__FR0), "1" (__FR1), "2" (__FR2)
        );
        
     return __FR2;       
}

/* SH4 fmac - floating-point multiply/accumulate */
/* Returns a*b+c at the cost of a single floating-point operation */
inline float FMAC( float a, float b, float c )
{
     register float __FR0 __asm__("fr0") = a; 
     register float __FR1 __asm__("fr1") = b; 
     register float __FR2 __asm__("fr2") = c;      
     
     __asm__ __volatile__( 
        "fmac   fr0, fr1, fr2\n"
        : "=f" (__FR0), "=f" (__FR1), "=f" (__FR2)
        : "0" (__FR0), "1" (__FR1), "2" (__FR2)
        );
        
     return __FR2;       
}

/* SH4 fmac - floating-point multiply/decrement */
/* Returns a*b-c at the cost of a single floating-point operation */
inline float FMDC( float a, float b, float c )
{
     register float __FR0 __asm__("fr0") = a; 
     register float __FR1 __asm__("fr1") = b; 
     register float __FR2 __asm__("fr2") = -c;      
     
     __asm__ __volatile__( 
        "fmac   fr0, fr1, fr2\n"
        : "=f" (__FR0), "=f" (__FR1), "=f" (__FR2)
        : "0" (__FR0), "1" (__FR1), "2" (__FR2)
        );
        
     return __FR2;       
}

inline float FABS( float n)
{
    register float __x __asm__("fr15") = n; 
	
	__asm__ __volatile__( 
		"fabs	fr15\n" 
		: "=f" (__x)
		: "0" (__x) );   
		
    return __x; 
}

inline float FSQRT( float n)
{
    register float __x __asm__("fr15") = n; 
	
	__asm__ __volatile__( 
		"fsqrt	fr15\n" 
		: "=f" (__x)
		: "0" (__x) );   
		
    return __x; 
}

#endif

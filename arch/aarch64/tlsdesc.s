// size_t __tlsdesc_static(size_t *a)
// {
// 	return a[1];
// }
.global __tlsdesc_static
.hidden __tlsdesc_static
.type __tlsdesc_static,@function
__tlsdesc_static:
	ldr x0,[x0,#8]
	ret

// size_t __tlsdesc_dynamic(size_t *a)
// {
// 	struct {size_t modidx,off;} *p = (void*)a[1];
// 	size_t *dtv = *(size_t**)(tp - 8);
// 	return dtv[p->modidx] + p->off - tp;
// }
.global __tlsdesc_dynamic
.hidden __tlsdesc_dynamic
.type __tlsdesc_dynamic,@function
__tlsdesc_dynamic:
	stp x1,x2,[sp,#-16]!
	ldr x0,[x0,#8]        // p
	ldp x2,x0,[x0]        // p->modidx, p->off
	ldp x1,x2,[sp],#16
	//mov x0, 0x680
	ret

#
# rep, repe (repz) and repne (repnz) prefixed string instructions
#   only count as one instruction, even though they repeat many times
# This test makes sure the bbv plugin counts these instructions properly
# The answer is validated to hw perf counters.
#

	.globl _start	
_start:	
	cld				# we want these to happen forward


	#===============================================
	# Some SSE2 instructions start with 0xf2 or 0xf3
	# Check for them, to make sure our rep detection
	#   handles things properly.
	# We should check this on x86 too, but then we'd
	#   have to check for SSE2 capability somehow?
	#===================================
false_positives:

	movdqu	%xmm1,%xmm2
	movdqu	%xmm2,%xmm1
	addsd	%xmm1,%xmm2
	pause

	#===================================
	# Check varied order of the size prefix
	#   with the rep prefix.  Older binutils
	#   did this one way, newer binutils the other
	#===================================
	
size_prefix:
	# test 16-bit load
	
	mov	$8192, %rcx
	mov	$buffer1, %rsi		# set source
	.byte 0x66, 0xf3, 0xad		# lodsw
	
	mov	$8192, %rcx
	mov	$buffer1, %rsi		# set source
	.byte 0xf3, 0x66, 0xad		# lodsw	
	
	


	#===================================
	# Load and Store Instructions
	#===================================
loadstore:
	xor	%rax, %rax
	mov	$0xd, %al		# set eax to d
	
	# test 8-bit store
	
	mov	$16384, %rcx
	mov	$buffer1, %rdi		# set destination
	rep	stosb	    		# store d 16384 times, auto-increment
	
	# test 8-bit load
	
	mov	$16384, %rcx
	mov	$buffer1, %rsi		# set source
	rep	lodsb	    		# load byte 16384 times, auto-increment

	cmp	$0xd,%al		# if we loaded wrong value
	jne	print_error		# print an error

	# test 16-bit store
	
	mov    	$0x020d,%ax		# store 0x020d
	
	mov	$8192, %rcx
	mov	$buffer1, %rdi		# set destination
	rep	stosw	    		# store 8192 times, auto-increment
	
	# test 16-bit load
	
	mov	$8192, %rcx
	mov	$buffer1, %rsi		# set source
	rep	lodsw	    		# load 8192 times, auto-increment

	cmp	$0x020d,%ax		# if we loaded wrong value
	jne	print_error		# print an error

	# test 32-bit store
	
	mov    	$0x0feb1378,%eax	# store 0x0feb1378
	
	mov	$4096, %rcx
	mov	$buffer1, %rdi		# set destination
	rep	stosl	    		# store 4096 times, auto-increment
	
	# test 32-bit load
	
	mov	$4096, %rcx
	mov	$buffer1, %rsi		# set source
	rep	lodsl	    		# load 4096 times, auto-increment

	cmp	$0x0feb1378,%eax	# if we loaded wrong value
	jne	print_error		# print an error
	
	# test 64-bit store
	
	mov    	$0xfeb131978a5a5a5a,%rax	
						
	mov	$2048, %rcx
	mov	$buffer1, %rdi		# set destination
	rep	stosq	    		# store 2048 times, auto-increment
	
	# test 64-bit load
	
	mov	$2048, %rcx
	mov	$buffer1, %rsi		# set source
	rep	lodsq	    		# load 2048 times, auto-increment

	cmp     $0x8a5a5a5a,%eax
					# !if we loaded wrong value
	jne	print_error		# print an error
	

	#=============================
	# Move instructions
	#=============================
moves:
	# test 8-bit move
	
	mov    $16384, %rcx
	mov    $buffer1, %rsi
	mov    $buffer2, %rdi
	rep    movsb

	# test 16-bit move
	
	mov    $8192, %rcx
	mov    $buffer2, %rsi
	mov    $buffer1, %rdi
	rep    movsw

	# test 32-bit move
	
	mov    $4096, %rcx
	mov    $buffer1, %rsi
	mov    $buffer2, %rdi
	rep    movsl	
	
	# test 64-bit move
	
	mov    $2048, %rcx
	mov    $buffer1, %rsi
	mov    $buffer2, %rdi
	rep    movsq		
	

	#==================================
	# Compare equal instructions
	#==================================
compare_equal:	
	# first set up the areas to compare
	
	mov	$0xa5a5a5a5,%eax
	mov	$buffer1, %rdi
	mov	$4096, %rcx
	rep	stosl
	
	mov	$0xa5a5a5a5,%eax
	mov	$buffer2, %rdi
	mov	$4096, %rcx
	rep	stosl


	# test 8-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$16384, %rcx
	repe	cmpsb
	jnz	print_error

	# test 16-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$8192, %rcx
	repe	cmpsw
	jnz	print_error

	# test 32-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$4096, %rcx
	repe	cmpsl
	jnz	print_error		
	
	# test 64-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$2048, %rcx
	repe	cmpsq
	jnz	print_error			



	#==================================
	# Compare not equal instructions
	#==================================
compare_noteq:	
	# change second buffer
	
	mov	$0x5a5a5a5a,%eax
	mov	$buffer2, %rdi
	mov	$4096, %rcx
	rep	stosl
	
	# test 8-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$16384, %rcx
#	repne	cmpsb             FIXME!  Not implemented valgrind
#	je	print_error

	# test 16-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$8192, %rcx
#	repne	cmpsw             FIXME!  Not implemented valgrind
#	je	print_error	

	# test 32-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$4096, %rcx
#	repne	cmpsl             FIXME!  Not implemented valgrind
#	je	print_error			

	# test 64-bit
	
	mov	$buffer1,%rsi
	mov	$buffer2,%rdi
	mov	$2048, %rcx
#	repne	cmpsq             FIXME!  Not implemented valgrind
#	je	print_error			

	#====================================
	# Check scan equal instruction
	#====================================
scan_eq:
	# test 8-bit

	mov     $0xa5,%al
	mov	$buffer1,%rdi
	mov	$16384, %rcx
	repe	scasb
	jnz	print_error

	# test 16-bit
	
	mov     $0xa5a5,%ax
	mov	$buffer1,%rdi
	mov	$8192, %rcx
	repe	scasw
	jnz	print_error	

	# test 32-bit
	
	mov	$0xa5a5a5a5,%eax
	mov	$buffer1,%rdi
	mov	$4096, %rcx
	repe	scasl
	jnz	print_error		
	
	# test 64-bit
	
	mov	$0xa5a5a5a5a5a5a5a5,%rax
	mov	$buffer1,%rdi
	mov	$2048, %rcx
	repe	scasq
	jnz	print_error			
	

	#====================================
	# Check scan not-equal instruction
	#====================================

	# test 8-bit
scan_ne:
	mov     $0xa5,%al
	mov	$buffer2,%rdi
	mov	$16384, %rcx
	repne	scasb
	jz	print_error

	# test 16-bit
	
	mov     $0xa5a5,%ax
	mov	$buffer2,%rdi
	mov	$8192, %rcx
	repne	scasw
	jz	print_error	
	
	# test 32-bit
	
	mov	$0xa5a5a5a5,%eax
	mov	$buffer2,%rdi
	mov	$4096, %rcx
	repne	scasl
	jz	print_error		
	
	# test 64-bit
	
	mov	$0xa5a5a5a5a5a5a5a5,%rax
	mov	$buffer2,%rdi
	mov	$2048, %rcx
	repne	scasq
	jz	print_error			

	jmp	exit			# no error, skip to exit
	
print_error:
	    
	mov 	$1, %rax		# Write syscall
	mov	$1, %rdi		# print to stdout
	mov	$error_string, %rsi	# string to print
	mov	$16, %edx      	   	# strlen
	syscall	 			# call syscall

	#================================
	# Exit
	#================================
exit:
     	mov	$60,%rax
	xor     %rdi,%rdi		# we return 0
	syscall             		# and exit


.data
error_string:	.asciz "Error detected!\n"

.bss

.lcomm	buffer1,	16384
.lcomm	buffer2,	16384

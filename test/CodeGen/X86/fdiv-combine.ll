; RUN: llc < %s -mtriple=x86_64-unknown-unknown -mcpu=x86-64 | FileCheck %s

; More than one 'arcp' division using a single divisor operand
; should be converted into a reciprocal and multiplication.

; Don't do anything for just one division.

define float @div1_arcp(float %x, float %y, float %z) {
; CHECK-LABEL: div1_arcp:
; CHECK:       # %bb.0:
; CHECK-NEXT:    divss %xmm1, %xmm0
; CHECK-NEXT:    retq
  %div1 = fdiv arcp float %x, %y
  ret float %div1
}

; All math instructions are 'arcp', so optimize.

define float @div2_arcp_all(float %x, float %y, float %z) {
; CHECK-LABEL: div2_arcp_all:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movss {{.*#+}} xmm3 = mem[0],zero,zero,zero
; CHECK-NEXT:    divss %xmm2, %xmm3
; CHECK-NEXT:    mulss %xmm3, %xmm0
; CHECK-NEXT:    mulss %xmm1, %xmm0
; CHECK-NEXT:    mulss %xmm3, %xmm0
; CHECK-NEXT:    retq
  %div1 = fdiv arcp float %x, %z
  %mul = fmul arcp float %div1, %y
  %div2 = fdiv arcp float %mul, %z
  ret float %div2
}

; The first division is not 'arcp', so do not optimize.

define float @div2_arcp_partial1(float %x, float %y, float %z) {
; CHECK-LABEL: div2_arcp_partial1:
; CHECK:       # %bb.0:
; CHECK-NEXT:    divss %xmm2, %xmm0
; CHECK-NEXT:    mulss %xmm1, %xmm0
; CHECK-NEXT:    divss %xmm2, %xmm0
; CHECK-NEXT:    retq
  %div1 = fdiv float %x, %z
  %mul = fmul arcp float %div1, %y
  %div2 = fdiv arcp float %mul, %z
  ret float %div2
}

; The second division is not 'arcp', so do not optimize.

define float @div2_arcp_partial2(float %x, float %y, float %z) {
; CHECK-LABEL: div2_arcp_partial2:
; CHECK:       # %bb.0:
; CHECK-NEXT:    divss %xmm2, %xmm0
; CHECK-NEXT:    mulss %xmm1, %xmm0
; CHECK-NEXT:    divss %xmm2, %xmm0
; CHECK-NEXT:    retq
  %div1 = fdiv arcp float %x, %z
  %mul = fmul arcp float %div1, %y
  %div2 = fdiv float %mul, %z
  ret float %div2
}

; The multiply is not 'arcp', but that does not prevent optimizing the divisions.

define float @div2_arcp_partial3(float %x, float %y, float %z) {
; CHECK-LABEL: div2_arcp_partial3:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movss {{.*#+}} xmm3 = mem[0],zero,zero,zero
; CHECK-NEXT:    divss %xmm2, %xmm3
; CHECK-NEXT:    mulss %xmm3, %xmm0
; CHECK-NEXT:    mulss %xmm1, %xmm0
; CHECK-NEXT:    mulss %xmm3, %xmm0
; CHECK-NEXT:    retq
  %div1 = fdiv arcp float %x, %z
  %mul = fmul float %div1, %y
  %div2 = fdiv arcp float %mul, %z
  ret float %div2
}

; If the reciprocal is already calculated, we should not
; generate an extra multiplication by 1.0. 

define double @div3_arcp(double %x, double %y, double %z) {
; CHECK-LABEL: div3_arcp:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movsd{{.*#+}} xmm2 = mem[0],zero
; CHECK-NEXT:    divsd %xmm1, %xmm2
; CHECK-NEXT:    mulsd %xmm2, %xmm0
; CHECK-NEXT:    addsd %xmm2, %xmm0
; CHECK-NEXT:    retq
  %div1 = fdiv fast double 1.0, %y
  %div2 = fdiv fast double %x, %y
  %ret = fadd fast double %div2, %div1
  ret double %ret
}

define float @div_select_constant_fold(i1 zeroext %arg) {
; CHECK-LABEL: div_select_constant_fold:
; CHECK:       # %bb.0:
; CHECK-NEXT:    testl %edi, %edi
; CHECK-NEXT:    jne .LBB6_1
; CHECK-NEXT:  # %bb.2:
; CHECK-NEXT:    movss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; CHECK-NEXT:    retq
; CHECK-NEXT:  .LBB6_1:
; CHECK-NEXT:    movss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; CHECK-NEXT:    retq
  %tmp = select i1 %arg, float 5.000000e+00, float 6.000000e+00
  %B2 = fdiv float %tmp, 1.000000e+00
  ret float %B2
}

define float @div_select_constant_fold_zero(i1 zeroext %arg) {
; CHECK-LABEL: div_select_constant_fold_zero:
; CHECK:       # %bb.0:
; CHECK-NEXT:    testl %edi, %edi
; CHECK-NEXT:    jne .LBB7_1
; CHECK-NEXT:  # %bb.2:
; CHECK-NEXT:    movss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; CHECK-NEXT:    jmp .LBB7_3
; CHECK-NEXT:  .LBB7_1:
; CHECK-NEXT:    movss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; CHECK-NEXT:  .LBB7_3:
; CHECK-NEXT:    xorps %xmm1, %xmm1
; CHECK-NEXT:    divss %xmm1, %xmm0
; CHECK-NEXT:    retq
  %tmp = select i1 %arg, float 5.000000e+00, float 6.000000e+00
  %B2 = fdiv float %tmp, 0.000000e+00
  ret float %B2
}

define void @PR24141() {
; CHECK-LABEL: PR24141:
; CHECK:	callq
; CHECK-NEXT:	divsd
; CHECK-NEXT:	jmp
entry:
  br label %while.body

while.body:
  %x.0 = phi double [ undef, %entry ], [ %div, %while.body ]
  %call = call { double, double } @g(double %x.0)
  %xv0 = extractvalue { double, double } %call, 0
  %xv1 = extractvalue { double, double } %call, 1
  %div = fdiv arcp double %xv0, %xv1
  br label %while.body
}

declare { double, double } @g(double)


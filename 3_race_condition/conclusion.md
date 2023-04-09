# CONCLUSION 

- It is observed that for smaller loop iterations, the value of mail turns out to be what was expected. However, when the iterations are 1 million or more, the value of mail turns out to be different than expected.
- This is because these operations will interleave between execution of threads. 
 
```
The routine function can be observed as doing three major operations:
1. read mails
2. increment 
3. write mails (in %eax)
```

> Study the threads.s file.
- In assembly code of routine on line 3(.L3), it can be observed that the value of mails is stored in the %eax register where synchronication is not maintained.  

---

## This condition is called RACE CONDITION 
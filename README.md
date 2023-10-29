# sparrow_vs
#### 1.修改

1. 修改的文件的编码格式

   因为文件编码不一样，在vs里面会警告，所以，修改了一下文件编码格式

2. 修改了一下OBJ_TO_VALUE定义

   ```c
   #define OBJ_TO_VALUE(objPtr) ({ \
      Value value; \
      value.type = VT_OBJ; \
      value.objHeader = (ObjHeader*)(objPtr); \
      value; \
   })
   ```

   ```c
   static Value OBJ_TO_VALUE(void* objPtr) {
       Value value; 
       value.type = VT_OBJ; 
      value.objHeader = (ObjHeader*)(objPtr); 
      return value; 
   }
   ```

   上面的语法再gcc里面可以运行，但是在vs会报错。这里改成静态函数。

3. 将内联类改成普通类

   ```c
   inline Class* getClassOfObj(VM* vm, Value object);
   ```

   ```
   Class* getClassOfObj(VM* vm, Value object);
   ```

   这里地方只是改了定义，还有一个函数的实现也需要改一下。

4. 改一下vm的申请内存的代码

   ```c
   VM* vm = (VM*)malloc(sizeof(VM)); 
   ```

   

   ```c
   VM* vm = (VM*)calloc(1,sizeof(VM));
   ```

   用malloc申请的内存不会初始化为0，后期会报错。这里改成calloc全部初始化为0就好。

   


#include "gc.h"
#include "compiler.h"
#include "obj_list.h"
#include "obj_range.h"
#if DEBUG
#include "debug.h"
#include <time.h>
#endif

//���obj:����obj�ռ�������vm->grays.grayObjects
void grayObject(VM* vm, ObjHeader* obj) {
    //���isDarkΪtrue��ʾΪ��ɫ,˵���Ѿ��ɴ�,ֱ�ӷ���
    if (obj == NULL || obj->isDark) return;

    //���Ϊ�ɴ�
    obj->isDark = true;

    //������������������
    if (vm->grays.count >= vm->grays.capacity) {
        vm->grays.capacity = vm->grays.count * 2;
        vm->grays.grayObjects =
            (ObjHeader**)realloc(vm->grays.grayObjects, vm->grays.capacity * sizeof(ObjHeader*));
    }

    //��obj��ӵ�����grayObjects
    vm->grays.grayObjects[vm->grays.count++] = obj;
}

//���value
void grayValue(VM* vm, Value value) {
    //ֻ�ж������Ҫ���
    if (!VALUE_IS_OBJ(value)) {
        return;
    }
    grayObject(vm, VALUE_TO_OBJ(value));
}

//���buffer->datas�е�value
static void grayBuffer(VM* vm, ValueBuffer* buffer) {
    uint32_t idx = 0;
    while (idx < buffer->count) {
        grayValue(vm, buffer->datas[idx]);
        idx++;
    }
}

//���class
static void blackClass(VM* vm, Class* class) {
    //���meta��
    grayObject(vm, (ObjHeader*)class->objHeader.class);

    //��Ҹ���
    grayObject(vm, (ObjHeader*)class->superClass);

    //��ҷ���
    uint32_t idx = 0;
    while (idx < class->methods.count) {
        if (class->methods.datas[idx].type == MT_SCRIPT) {
            grayObject(vm, (ObjHeader*)class->methods.datas[idx].obj);
        }
        idx++;
    }

    //�������
    grayObject(vm, (ObjHeader*)class->name);

    //�ۼ����С
    vm->allocatedBytes += sizeof(Class);
    vm->allocatedBytes += sizeof(Method) * class->methods.capacity;
}

//��ұհ�
static void blackClosure(VM* vm, ObjClosure* objClosure) {
    //��ұհ��еĺ���
    grayObject(vm, (ObjHeader*)objClosure->fn);

    //��Ұ��е�upvalue
    uint32_t idx = 0;
    while (idx < objClosure->fn->upvalueNum) {
        grayObject(vm, (ObjHeader*)objClosure->upvalues[idx]);
        idx++;
    }

    //�ۼƱհ���С
    vm->allocatedBytes += sizeof(ObjClosure);
    vm->allocatedBytes += sizeof(ObjUpvalue*) * objClosure->fn->upvalueNum;
}

//���objThread
static void blackThread(VM* vm, ObjThread* objThread) {
    //���frame
    uint32_t idx = 0;
    while (idx < objThread->usedFrameNum) {
        grayObject(vm, (ObjHeader*)objThread->frames[idx].closure);
        idx++;
    }

    //�������ʱջ��ÿ��slot
    Value* slot = objThread->stack;
    while (slot < objThread->esp) {
        grayValue(vm, *slot);
        slot++;
    }

    //��ұ��߳������е�upvalue
    ObjUpvalue* upvalue = objThread->openUpvalues;
    while (upvalue != NULL) {
        grayObject(vm, (ObjHeader*)upvalue);
        upvalue = upvalue->next;
    }

    //���caller
    grayObject(vm, (ObjHeader*)objThread->caller);
    grayValue(vm, objThread->errorObj);

    //�ۼ��̴߳�С
    vm->allocatedBytes += sizeof(ObjThread);
    vm->allocatedBytes += objThread->frameCapacity * sizeof(Frame);
    vm->allocatedBytes += objThread->stackCapacity * sizeof(Value);
}

//���fn
static void blackFn(VM* vm, ObjFn* fn) {
    //��ҳ���
    grayBuffer(vm, &fn->constants);

    //�ۼ�Objfn�Ŀռ�
    vm->allocatedBytes += sizeof(ObjFn);
    vm->allocatedBytes += sizeof(uint8_t) * fn->instrStream.capacity;
    vm->allocatedBytes += sizeof(Value) * fn->constants.capacity;

#if DEBUG  
    //�ټ���debug��Ϣռ�õ��ڴ�
    vm->allocatedBytes += sizeof(Int) * fn->instrStream.capacity;
#endif  
}

//���objInstance
static void blackInstance(VM* vm, ObjInstance* objInstance) {
    //���Ԫ��
    grayObject(vm, (ObjHeader*)objInstance->objHeader.class);

    //���ʵ����������,��ĸ�����class->fieldNum
    uint32_t idx = 0;
    while (idx < objInstance->objHeader.class->fieldNum) {
        grayValue(vm, objInstance->fields[idx]);
        idx++;
    }

    //�ۼ�objInstance�ռ�
    vm->allocatedBytes += sizeof(ObjInstance);
    vm->allocatedBytes += sizeof(Value) * objInstance->objHeader.class->fieldNum;
}

//���objList
static void blackList(VM* vm, ObjList* objList) {
    //���list��elements
    grayBuffer(vm, &objList->elements);

    //�ۼ�objList��С
    vm->allocatedBytes += sizeof(ObjList);
    vm->allocatedBytes += sizeof(Value) * objList->elements.capacity;
}

//���objMap
static void blackMap(VM* vm, ObjMap* objMap) {
    //�������entry
    uint32_t idx = 0;
    while (idx < objMap->capacity) {
        Entry* entry = &objMap->entries[idx];
        //������Ч��entry
        if (!VALUE_IS_UNDEFINED(entry->key)) {
            grayValue(vm, entry->key);
            grayValue(vm, entry->value);
        }
        idx++;
    }

    //�ۼ�ObjMap��С
    vm->allocatedBytes += sizeof(ObjMap);
    vm->allocatedBytes += sizeof(Entry) * objMap->capacity;
}

//���objModule
static void blackModule(VM* vm, ObjModule* objModule) {
    //���ģ��������ģ�����
    uint32_t idx = 0;
    while (idx < objModule->moduleVarValue.count) {
        grayValue(vm, objModule->moduleVarValue.datas[idx]);
        idx++;
    }

    //���ģ����
    grayObject(vm, (ObjHeader*)objModule->name);

    //�ۼ�ObjModule��С
    vm->allocatedBytes += sizeof(ObjModule);
    vm->allocatedBytes += sizeof(String) * objModule->moduleVarName.capacity;
    vm->allocatedBytes += sizeof(Value) * objModule->moduleVarValue.capacity;
}

//���range
static void blackRange(VM* vm) {
    //ObjRange��û�д�����,ֻ��from��to,
    //��ռ�����sizeof(ObjRange),��˲��ö�����
    vm->allocatedBytes += sizeof(ObjRange);
}

//���objString
static void blackString(VM* vm, ObjString* objString) {
    //�ۼ�ObjString�ռ� +1�ǽ�β��'\0'
    vm->allocatedBytes += sizeof(ObjString) + objString->value.length + 1;
}

//���objUpvalue
static void blackUpvalue(VM* vm, ObjUpvalue* objUpvalue) {
    //���objUpvalue��closedUpvalue
    grayValue(vm, objUpvalue->closedUpvalue);

    //�ۼ�objUpvalue��С
    vm->allocatedBytes += sizeof(ObjUpvalue);
}

//���obj
static void blackObject(VM* vm, ObjHeader* obj) {
#ifdef DEBUG
    printf("mark ");
    dumpValue(OBJ_TO_VALUE(obj));
    printf(" @ %p\n", obj);
#endif
    //���ݶ������ͷֱ���
    switch (obj->type) {
    case OT_CLASS:
        blackClass(vm, (Class*)obj);
        break;

    case OT_CLOSURE:
        blackClosure(vm, (ObjClosure*)obj);
        break;

    case OT_THREAD:
        blackThread(vm, (ObjThread*)obj);
        break;

    case OT_FUNCTION:
        blackFn(vm, (ObjFn*)obj);
        break;

    case OT_INSTANCE:
        blackInstance(vm, (ObjInstance*)obj);
        break;

    case OT_LIST:
        blackList(vm, (ObjList*)obj);
        break;

    case OT_MAP:
        blackMap(vm, (ObjMap*)obj);
        break;

    case OT_MODULE:
        blackModule(vm, (ObjModule*)obj);
        break;

    case OT_RANGE:
        blackRange(vm);
        break;

    case OT_STRING:
        blackString(vm, (ObjString*)obj);
        break;

    case OT_UPVALUE:
        blackUpvalue(vm, (ObjUpvalue*)obj);
        break;
    }
}

//�����Щ�Ѿ���ҵĶ���,��������Щ��ҵĶ���
static void blackObjectInGray(VM* vm) {
    //����Ҫ�����Ķ����Ѿ��ռ�����vm->grays.grayObjects��,
    //������һ���
    while (vm->grays.count > 0) {
        ObjHeader* objHeader = vm->grays.grayObjects[--vm->grays.count];
        blackObject(vm, objHeader);
    }
}

//�ͷ�obj������ռ�õ��ڴ�
void freeObject(VM* vm, ObjHeader* obj) {
#ifdef DEBUG 
    printf("free ");
    dumpValue(OBJ_TO_VALUE(obj));
    printf(" @ %p\n", obj);
#endif

    //���ݶ������ͷֱ���
    switch (obj->type) {
    case OT_CLASS:
        MethodBufferClear(vm, &((Class*)obj)->methods);
        break;

    case OT_THREAD: {
        ObjThread* objThread = (ObjThread*)obj;
        DEALLOCATE(vm, objThread->frames);
        DEALLOCATE(vm, objThread->stack);
        break;
    }

    case OT_FUNCTION: {
        ObjFn* fn = (ObjFn*)obj;
        ValueBufferClear(vm, &fn->constants);
        ByteBufferClear(vm, &fn->instrStream);
#if DEBUG
        IntBufferClear(vm, &fn->debug->lineNo);
        DEALLOCATE(vm, fn->debug->fnName);
        DEALLOCATE(vm, fn->debug);
#endif
        break;
    }

    case OT_LIST:
        ValueBufferClear(vm, &((ObjList*)obj)->elements);
        break;

    case OT_MAP:
        DEALLOCATE(vm, ((ObjMap*)obj)->entries);
        break;

    case OT_MODULE:
        StringBufferClear(vm, &((ObjModule*)obj)->moduleVarName);
        ValueBufferClear(vm, &((ObjModule*)obj)->moduleVarValue);
        break;

    case OT_STRING:
    case OT_RANGE:
    case OT_CLOSURE:
    case OT_INSTANCE:
    case OT_UPVALUE:
        break;
    }

    //������ͷ��Լ�
    DEALLOCATE(vm, obj);
}

//������������������ȥ�ͷ�δ�õ��ڴ�
void startGC(VM* vm) {
#ifdef DEBUG 
    double startTime = (double)clock() / CLOCKS_PER_SEC;
    uint32_t before = vm->allocatedBytes;
    printf("-- gc  before:%d   nextGC:%d  vm:%p  --\n",
        before, vm->config.nextGC, vm);
#endif
    // һ ��ǽ׶�:�����Ҫ�����Ķ���

       //��allocatedBytes��0���ھ�ȷͳ�ƻ��պ���ܷ����ڴ��С
    vm->allocatedBytes = 0;

    //allModules���ܱ��ͷ�
    grayObject(vm, (ObjHeader*)vm->allModules);

    //���tmpRoots�����еĶ���(���ɴﵫ�ǲ��뱻����,������)
    uint32_t idx = 0;
    while (idx < vm->tmpRootNum) {
        grayObject(vm, vm->tmpRoots[idx]);
        idx++;
    }

    //��ҵ�ǰ�߳�,���ܱ�����
    grayObject(vm, (ObjHeader*)vm->curThread);

    //�����������������ڴ���߾ͱ�ұ��뵥Ԫ
    if (vm->curParser != NULL) {
        ASSERT(vm->curParser->curCompileUnit != NULL,
            "grayCompileUnit only be called while compiling!");
        grayCompileUnit(vm, vm->curParser->curCompileUnit);
    }

    //�ú����лҶ���(�����Ķ���)
    blackObjectInGray(vm);

    // �� ��ɨ�׶�:���հ׶���(��������)

    ObjHeader** obj = &vm->allObjects;
    while (*obj != NULL) {
        //���հ׶���
        if (!((*obj)->isDark)) {
            ObjHeader* unreached = *obj;
            *obj = unreached->next;
            freeObject(vm, unreached);
        }
        else {
            //����Ѿ��Ǻڶ���,Ϊ����һ��gc�����ж�,
            //���ڽ���ָ�Ϊδ���״̬,������Զ��������
            (*obj)->isDark = false;
            obj = &(*obj)->next;
        }
    }

    //������һ�δ���gc�ķ�ֵ
    vm->config.nextGC = vm->allocatedBytes * vm->config.heapGrowthFactor;
    if (vm->config.nextGC < vm->config.minHeapSize) {
        vm->config.nextGC = vm->config.minHeapSize;
    }

#ifdef DEBUG
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - startTime;
    printf("GC %lu before, %lu after (%lu collected), next at %lu. take %.3fs.\n",
        (unsigned long)before,
        (unsigned long)vm->allocatedBytes,
        (unsigned long)(before - vm->allocatedBytes),
        (unsigned long)vm->config.nextGC,
        elapsed);
#endif
}

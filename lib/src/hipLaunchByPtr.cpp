// MIT License
//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*  
   hipLaunchByPtr.cpp defines these host-callable external hip functions.  
   References to these functions are generated by the clang compiler when 
   the hip language is activated with the "-x hip" option AND
   the triple chevron <<<>>> syntax is used to launch kernels.

      hipConfigureCall
      hipLaunchByPtr
      hipSetupArgument
      __hipRegisterFatBinary
      __hipRegisterFunction
      __hipRegisterVar
      __hipUnregisterFatBinary

   This code uses these hip functions not defined here.
   They are declared in <hip/hip_runtime_api.h  
      hipCtxCreate
      hipDeviceGet
      hipDeviceSynchronize
      hipGetDeviceProperties
      hipGetErrorString
      hipInit
      hipModuleGetFunction
      hipModuleLaunchKernel
      hipModuleLoadData
*/

#define __HIPCC__ 1
#define __host__
#define __device__ 
#include <hip/hip_runtime_api.h>

#include "hipLaunchByPtr.h"

struct hipi_global_s     * hipi_Global;
struct hipi_launchdata_s * hipi_Ldata;
struct hipi_kernel_s     * hipi_Kernels;

bool hipi_EnableDebug = false;
// extern bool hipi_EnableDebug;

#define DEBUG(X)                      \
  do { if (hipi_EnableDebug) { X; } \
  } while (false)

#define ErrorCheckHIP(msg, status) \
if (status != hipSuccess) { \
  hipi_dbgs() << #msg << " failed (" << hipGetErrorString(status) << ") <" <<  \
    status << ">\n"; \
    exit(1); \
} else { \
  DEBUG(hipi_dbgs() << #msg << " succeeded.\n"); \
}

#define ErrorCheck(msg, status) \
if (status != HSA_STATUS_SUCCESS) { \
  hipi_dbgs() << #msg << " failed.\n"; \
    exit(1); \
} else { \
  hipi_dbgs() << #msg << " succeeded.\n");  \
}

// --- Start of Internal functions hipi_  
extern "C" std::ostream& hipi_dbgs() { return std::cout;}

// hipi_GetSubarch is the 1st routine called by __hipRegisterFatBinary.
// It determines the subarch for the obj to retreive from fatbin.
// So this is called before the fatbinary is scanned.
extern "C" hipi_err_t hipi_GetSubarch(hipi_global_t* glob, unsigned int* subarch){
  hipError_t hip_error;

  ////// STUB (TEST HIP)
  hip_error = hipInit(0);
  ErrorCheckHIP("Initializing HIP", hip_error);
  if(hip_error != hipSuccess)
    return HIPBYPTR_FAIL;

  hip_error = hipDeviceGet(&glob->hip_device, 0);
  ErrorCheckHIP("HIP device get", hip_error);

  hip_error = hipCtxCreate(&glob->hip_context, 0, glob->hip_device);
  ErrorCheckHIP("HIP Ctx Create", hip_error);

  hipDeviceProp_t hipDevProp;
  hip_error = hipGetDeviceProperties(&hipDevProp, glob->hip_device);
  ErrorCheckHIP("HIP device get property", hip_error);

  DEBUG(hipi_dbgs() << "sizeof(hipIpcMemHandle_t): " <<
        sizeof(hipIpcMemHandle_t) << "\n");
  DEBUG(hipi_dbgs() << "sizeof(hipIpcEventHandle_t): " <<
        sizeof(hipIpcEventHandle_t) << "\n");
//  DEBUG(hipi_dbgs() << "sizeof(hipChannelFormatDesc): " <<
//        sizeof(hipChannelFormatDesc) << "\n");
  DEBUG(hipi_dbgs() << "sizeof(hipDeviceProp_t): " <<
        sizeof(hipDeviceProp_t) << "\n");

  *subarch = hipDevProp.gcnArch;
  DEBUG(hipi_dbgs() << "Subarch: " <<
        *subarch << "\n");

  if(*subarch != 0) {
    return HIPBYPTR_SUCCESS;
  } else {
    return HIPBYPTR_FAIL;
  }
}

// hipi_InitGlobal is called after hipi_getObjForSubarch gets the
// raw object from fatbin.  InitHsa routine loads the object as an hsa
// code object and sets up the executable.  This is the 3rd routine
// called by __hipRegisterFatBinary.
extern "C" hipi_err_t hipi_InitGlobal(hipi_global_t* glob, void* raw_hsaco, unsigned objsz){

  //  Initialize HSA queues to zero length
  glob->syncq_len = 0 ;
  unsigned i;
  for(i=0 ; i<HIP_MAX_STREAMS ; i++) glob->streamq_len[i]=0;

  hipError_t hip_error;
    hip_error = hipModuleLoadData(&glob->hip_module, raw_hsaco);

  ErrorCheckHIP("HIP hipModuleLoadData", hip_error);
  DEBUG(hipi_dbgs() << "DEBUG: Object size: " << objsz << "\n");

  return HIPBYPTR_SUCCESS;
}

// This function is called by __hipRegisterFunction
extern "C" hipi_err_t hipi_RegisterKernel(
  hipi_global_t   * global,
  hipi_kernel_t   * kernel
){
  hipi_err_t rc = 0;
  return rc;
}

extern "C" hipError_t hipi_Launch(
  hipi_global_t     * global,
  hipi_kernel_t     * kernel,
  hipi_launchdata_t * ldata
) {
  dim3  gridDim  = ldata->gridDim;
  dim3  blockDim = ldata->blockDim;
  const char* kernelName = kernel->kernel_name;
  size_t argstruct_size = ldata->argstructsize; // Note: This
                                                // parameter seems to
                                                // be igonred, using
                                                // other values makes
                                                // no difference.
  volatile void *config[] = {
    HIP_LAUNCH_PARAM_BUFFER_POINTER, (void*)ldata->argstruct,
    HIP_LAUNCH_PARAM_BUFFER_SIZE, &argstruct_size,
    HIP_LAUNCH_PARAM_END
  };

  hipFunction_t function;
  hipError_t hip_error = hipModuleGetFunction(&function, global->hip_module, kernel->kernel_name);
  ErrorCheckHIP("GetFunction", hip_error);
  DEBUG(hipi_dbgs() << "Function: " << function << "\n");
  DEBUG(hipi_dbgs() << "LAUNCHING KERNEL: " << kernel->kernel_name << "\n");
  DEBUG(hipi_dbgs() << "Argument size: " << argstruct_size << "\n");
  DEBUG(hipi_dbgs() << "GRID(" << gridDim.x << ", " <<  gridDim.y << ", " <<
        gridDim.z << "\n");
  DEBUG(hipi_dbgs() << "BLOCK(" << blockDim.x << ", " <<  blockDim.y << ", " <<
        blockDim.z << "\n");

  int gridX = gridDim.x;
  int gridY = gridDim.y;
  int gridZ = gridDim.z;

  hipError_t rc = hipModuleLaunchKernel(
    function,
    gridX,
    gridY,
    gridZ,
    blockDim.x,
    blockDim.y,
    blockDim.z,
    ldata->smsize,
    0, // Use default stream for now
    NULL, // kernelParams Not implemented in HIP
    (void**)config);
  ErrorCheckHIP("hipModuleLaunchKernel", rc);
  hipDeviceSynchronize();
  return rc;
}

#define COPY_FIELD(Dest, Source, FieldName) \
  memcpy(((char*)(&Dest[0])+DevicePropOffset::FieldName),\
         &(Source.FieldName), \
         sizeof(Source.FieldName))

extern "C" hipi_err_t hipi_GetDeviceProperties(HIP_DeviceProp* devProp, int devId) {
  hipDeviceProp_t hipDevProp;
    hipi_err_t rc = 0;

    rc = (hipi_err_t) hipGetDeviceProperties(&hipDevProp, devId);

    COPY_FIELD(devProp, hipDevProp, name);
    COPY_FIELD(devProp, hipDevProp, totalGlobalMem);
    COPY_FIELD(devProp, hipDevProp, sharedMemPerBlock);
    COPY_FIELD(devProp, hipDevProp, regsPerBlock);
    COPY_FIELD(devProp, hipDevProp, warpSize);
    COPY_FIELD(devProp, hipDevProp, maxThreadsPerBlock);
    COPY_FIELD(devProp, hipDevProp, maxThreadsDim);
    COPY_FIELD(devProp, hipDevProp, maxGridSize);
    COPY_FIELD(devProp, hipDevProp, clockRate);
    COPY_FIELD(devProp, hipDevProp, memoryClockRate);
    COPY_FIELD(devProp, hipDevProp, totalConstMem);
    COPY_FIELD(devProp, hipDevProp, major);
    COPY_FIELD(devProp, hipDevProp, minor);
    COPY_FIELD(devProp, hipDevProp, multiProcessorCount);
    COPY_FIELD(devProp, hipDevProp, l2CacheSize);
    COPY_FIELD(devProp, hipDevProp, maxThreadsPerMultiProcessor);
    COPY_FIELD(devProp, hipDevProp, computeMode);
    COPY_FIELD(devProp, hipDevProp, concurrentKernels);
    COPY_FIELD(devProp, hipDevProp, pciBusID);
    COPY_FIELD(devProp, hipDevProp, pciDeviceID);
    COPY_FIELD(devProp, hipDevProp, isMultiGpuBoard);
    COPY_FIELD(devProp, hipDevProp, canMapHostMemory);

    return rc;
}

extern "C" void hipi_InitDebug() {
  const char* nccl_debug = getenv("HIP_DEBUG");
  if (nccl_debug == NULL) {
    hipi_EnableDebug = false;
  } else if (strcmp(nccl_debug, "0") == 0) {
    hipi_EnableDebug = false;
  } else {
    hipi_EnableDebug = true;
  }
}

#define __hipByPtrFatMAGIC  0x1ee55a01
#define __hipByPtrFatMAGIC2 0x466243b1
#define __hipByPtrFatMAGIC3 0xba55ed50

extern "C" void* hipi_GetObjForSubarch(void* fatbin, unsigned subarch, unsigned * objsz) {

  DEBUG(hipi_dbgs() << "\n==>DEBUG: hipi_GetObjForSubarch\n");
  void* hsaco = nullptr;

  hipi_fbwrapper* fbwrapper = (hipi_fbwrapper*)fatbin;
  if (fbwrapper->magic != __hipByPtrFatMAGIC2 || fbwrapper->version != 1) {
    DEBUG(hipi_dbgs() << "Not a valid fatbin wrapper!\n");
    return NULL;
  }

  hipi_fbheader* fbheader = (hipi_fbheader*)fbwrapper->binary;
  if (fbheader->magic != __hipByPtrFatMAGIC3 || fbheader->version != 1) {
    DEBUG(hipi_dbgs() << "Not a valid fatbin!\n");
    return NULL;
  }

  hipi_partheader* pheader = (hipi_partheader*)((uintptr_t)fbheader + fbheader->headerSize);
  hipi_partheader* end = (hipi_partheader*)((uintptr_t)pheader + fbheader->fatSize);

  while (pheader < end) {
    DEBUG(hipi_dbgs() << "part has a leading value of "
          << *((unsigned*)((uintptr_t)pheader + pheader->headerSize))
          << " elfvalue is " << 0x464c457f << " length: " << pheader->partSize << "\n");
    DEBUG(hipi_dbgs() << "Subarch: " << pheader->subarch << "\n");
    if (pheader->subarch == subarch) {
      if (objsz) {
        *objsz = pheader->partSize;
      }
      hsaco = (void*)((uintptr_t)pheader + pheader->headerSize);
      break;
    }
    pheader = (hipi_partheader*)((uintptr_t)pheader + pheader->headerSize + pheader->partSize);
  }

  DEBUG(hipi_dbgs() <<  "<==EXIT: hipi_GetObjForSubarch\n");

  return hsaco;
}

/* ------------  End hip internal functions hipi_ ----------------------------*/

extern "C" EXPORT void ** __hipRegisterFatBinary(
  void* fatbin) {

  hipi_err_t rc;
  unsigned int objsz;
  unsigned int subarch;
  void * raw_hsaco;
  hipi_InitDebug();
  hipi_Global = (hipi_global_t*) calloc(sizeof(hipi_global_s),1);
  hipi_Global->kernel_count = 0 ;
  hipi_Ldata = (hipi_launchdata_t*) calloc(sizeof(hipi_launchdata_s),1);

  // GetSubArch also calls hsa_init
  rc = hipi_GetSubarch(hipi_Global, &subarch);

  raw_hsaco = hipi_GetObjForSubarch(fatbin, subarch, &objsz);

  DEBUG(hipi_dbgs() << "\n\n==>DEBUG: __hipRegisterFat Binary \n");
  DEBUG(hipi_dbgs() << " Object size: " << objsz << ", Subarch: " << subarch
        << "\n");

  if (!raw_hsaco) {
    DEBUG(hipi_dbgs() << "Could not retrieve HSA Code Object for agent " <<
          hipi_Global->agent_name << "subarch: " << subarch << "\n");
     exit(1);
  }

  rc = hipi_InitGlobal(hipi_Global, raw_hsaco, objsz);

  // We should try to get the actual number of kernels from LoadObj
  hipi_Kernels = (hipi_kernel_t*) malloc(sizeof(hipi_kernel_s)*HIP_MAX_KERNELS);

  return (void**) VALCHECK;
}

extern "C" EXPORT void __hipRegisterVar(
  void **fatCubinHandle,
  char  *hostVar,
  char  *deviceAddress,
  const char  *deviceName,
  int    ext,
  int    size,
  int    constant,
  int    global) {
  // STUB
}

extern "C" EXPORT void __hipRegisterFunction(
  unsigned    ** fatbin,
  void*          khaddr,
  char         * host_name,
  const char   * kernel_name,
  unsigned int   thread_limit,
  uint3        * tid,
  uint3        * bid,
  dim3         * bDim,
  dim3         * gDim,
  int          * wSize
){
  unsigned int KID = (hipi_Global->kernel_count)++;

  hipi_Kernels[KID].khaddr        = khaddr;
  hipi_Kernels[KID].host_name     = host_name;
  hipi_Kernels[KID].kernel_name   = kernel_name;
  hipi_Kernels[KID].thread_limit  = thread_limit;
  hipi_Kernels[KID].tid           = tid;
  hipi_Kernels[KID].bid           = bid;
  hipi_Kernels[KID].bDim          = bDim;
  hipi_Kernels[KID].gDim          = gDim;
  hipi_Kernels[KID].wSize         = wSize;

  DEBUG(hipi_dbgs() << "\n==>DEBUG: __hipRegisterFunction \n   host_name: " <<
        host_name << "\nkernel_name: " << kernel_name << "\n");
  DEBUG(hipi_dbgs() << "   tl:" << thread_limit << "KernelId: " << KID <<
        "khaddr: " << khaddr << "wSize: "<< wSize << "\n");

  hipi_err_t rc = hipi_RegisterKernel(hipi_Global, &hipi_Kernels[KID]);

  return;
}

// declare zeroext i32 @hipConfigureCall([2 x i64], [2 x i64], i64,
// %struct.CUstream_st*) local_unnamed_addr #0
extern "C" EXPORT hipError_t hipConfigureCall(
  dim3 gridDim,
  dim3 blockDim,
  long long smsize,
  hipStream_t * stream
){

  //  Add lock here to serialize access to hipi_Ldata
  hipi_Ldata->gridDim  = gridDim;
  hipi_Ldata->blockDim = blockDim;
  hipi_Ldata->smsize   = smsize;
  hipi_Ldata->stream   = stream;
  hipi_Ldata->argstructsize   = 0;

  DEBUG(hipi_dbgs() << "\n==>DEBUG: hipConfigureCall called size=" << smsize <<
        " stream: " <<  stream << "\n");
  DEBUG(hipi_dbgs() << "==>DEBUG:    gridDim  (" << hipi_Ldata->gridDim.x << ", "
        << hipi_Ldata->gridDim.y << ", " << hipi_Ldata->gridDim.z << ") \n");
  DEBUG(hipi_dbgs() << "==>DEBUG:    blockDim (" << hipi_Ldata->blockDim.x << ", "
        << hipi_Ldata->blockDim.y << ",  "
        << hipi_Ldata->blockDim.z << ") \n");

  hipError_t rc = hipSuccess ;
  return rc;
}

// declare i32 @hipSetupArgument(i8*, i64, i64) local_unnamed_addr
extern "C" EXPORT hipError_t hipSetupArgument(
  unsigned long long * arg,
  unsigned long long size,
  unsigned long long offset) {
  memcpy((hipi_Ldata->argstruct + offset),arg,size);
  hipi_Ldata->argstructsize += offset + size;

  if ( size == 4 ) {
    unsigned value = (unsigned) *arg;
    DEBUG(hipi_dbgs() << "==>DEBUG:    hipSetupArgument arg: " << arg <<
          " value: " << value << " (0x" << std::hex << value << std::dec << ") with size " << size << " off: " << offset <<
          "\n");
  } else
    DEBUG(hipi_dbgs() << "==>DEBUG:    hipSetupArgument arg: " << arg << " value: "
          << *arg << " (0x" << std::hex << *arg << std::dec << ") with size " << size << " off: " << offset << "\n");

  return hipSuccess;
}

// khaddr is the symbol address offset into the host text elf section
// for the host function that launches the kernel.  khaddr is as good
// an kernel identifier as anything else. We call this "khaddr".  The
// host function at this address is fairly simple, it calls
// hipSetupArgument for each arg then callse @hipLaunchByPtr.  So khaddr
// is actually a traceback to the host function calling hipLaunchByPtr.
// We have this

// declare i32 @hipLaunchByPtr(i8*) local_unnamed_addr
extern "C" EXPORT hipError_t hipLaunchByPtr(long long * khaddr){

  // Find the KID
  unsigned int KID,i;
  for (i = 0 ; i<hipi_Global->kernel_count ; i++)
    if( hipi_Kernels[i].khaddr == khaddr) KID=i;

  DEBUG(hipi_dbgs() << "==>DEBUG:    hipLaunchByPtr for " <<
        hipi_Kernels[KID].kernel_name << " kernel number: " <<
        KID << " khaddr: " << khaddr << "\n");

  return hipi_Launch(hipi_Global, &hipi_Kernels[KID], hipi_Ldata);
}

extern "C" EXPORT void ** __hipUnregisterFatBinary( void** fatbin){
  if (hipi_Global != 0) {
    free(hipi_Global);
    hipi_Global = 0;
  }
  
  DEBUG(hipi_dbgs() << "==>DEBUG: __hipUnregisterFatBinary called for " << fatbin
        << "\n");
  return NULL;
}


/** @file
  This will be invoked only once. It will call FspMemoryInit API,
  register TemporaryRamDonePpi to call TempRamExit API, and register MemoryDiscoveredPpi
  notify to call FspSiliconInit API.

  Copyright (c) 2014 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include <PiPei.h>

#include <Library/PeimEntryPoint.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeiServicesTablePointerLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/PerformanceLib.h>
#include <Library/FspWrapperPlatformLib.h>
#include <Library/FspWrapperHobProcessLib.h>
#include <Library/FspWrapperApiLib.h>

#include <Ppi/FspSiliconInitDone.h>
#include <Ppi/EndOfPeiPhase.h>
#include <Ppi/MemoryDiscovered.h>
#include <Ppi/SecPlatformInformation.h>
#include <Library/PlatformSecLib.h>
#include <Library/FspWrapperApiTestLib.h>
#include <FspEas.h>

extern EFI_GUID gFspHobGuid;

/**
  Call FspMemoryInit API.

  @return Status returned by FspMemoryInit API.
**/
EFI_STATUS
PeiFspMemoryInit (
  VOID
  )
{
  FSP_INFO_HEADER           *FspmHeaderPtr;
  EFI_STATUS                Status;
  UINT64                    TimeStampCounterStart;
  VOID                      *FspHobListPtr;
  VOID                      *HobData;
  FSPM_UPD_COMMON           *FspmUpdDataPtr;
  UINTN                     *SourceData;

  DEBUG ((DEBUG_INFO, "PeiFspMemoryInit enter\n"));

  FspHobListPtr = NULL;

  //
  // Copy default FSP-M UPD data from Flash
  //
  FspmHeaderPtr = (FSP_INFO_HEADER *)FspFindFspHeader (PcdGet32 (PcdFspmBaseAddress));
  FspmUpdDataPtr = (FSPM_UPD_COMMON *)AllocateZeroPool ((UINTN)FspmHeaderPtr->CfgRegionSize);
  ASSERT (FspmUpdDataPtr != NULL);
  SourceData = (UINTN *)((UINTN)FspmHeaderPtr->ImageBase + (UINTN)FspmHeaderPtr->CfgRegionOffset);
  CopyMem (FspmUpdDataPtr, SourceData, (UINTN)FspmHeaderPtr->CfgRegionSize);

  DEBUG ((DEBUG_INFO, "UpdateFspmUpdData enter\n"));
  UpdateFspmUpdData ((VOID *)FspmUpdDataPtr);
  DEBUG ((DEBUG_INFO, "  NvsBufferPtr        - 0x%x\n", FspmUpdDataPtr->FspmArchUpd.NvsBufferPtr));
  DEBUG ((DEBUG_INFO, "  StackBase           - 0x%x\n", FspmUpdDataPtr->FspmArchUpd.StackBase));
  DEBUG ((DEBUG_INFO, "  StackSize           - 0x%x\n", FspmUpdDataPtr->FspmArchUpd.StackSize));
  DEBUG ((DEBUG_INFO, "  BootLoaderTolumSize - 0x%x\n", FspmUpdDataPtr->FspmArchUpd.BootLoaderTolumSize));
  DEBUG ((DEBUG_INFO, "  BootMode            - 0x%x\n", FspmUpdDataPtr->FspmArchUpd.BootMode));
  DEBUG ((DEBUG_INFO, "  HobListPtr          - 0x%x\n", &FspHobListPtr));

  TimeStampCounterStart = AsmReadTsc ();
  Status = CallFspMemoryInit (FspmUpdDataPtr, &FspHobListPtr);
  // Create hobs after memory initialization and not in temp RAM. Hence passing the recorded timestamp here
  PERF_START_EX(&gFspApiPerformanceGuid, "EventRec", NULL, TimeStampCounterStart, 0xD000);
  PERF_END_EX(&gFspApiPerformanceGuid, "EventRec", NULL, 0, 0xD07F);
  DEBUG ((DEBUG_INFO, "Total time spent executing FspMemoryInitApi: %d millisecond\n", DivU64x32 (GetTimeInNanoSecond (AsmReadTsc () - TimeStampCounterStart), 1000000)));
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR - Failed to execute FspMemoryInitApi(), Status = %r\n", Status));
  }
  DEBUG((DEBUG_INFO, "FspMemoryInit status: 0x%x\n", Status));
  ASSERT_EFI_ERROR (Status);


  Status = TestFspMemoryInitApiOutput (FspmUpdDataPtr, &FspHobListPtr);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR - TestFspMemoryInitApiOutput () fail, Status = %r\n", Status));
  }

  DEBUG ((DEBUG_INFO, "  FspHobListPtr (returned) - 0x%x\n", FspHobListPtr));
  ASSERT (FspHobListPtr != NULL);

  PostFspmHobProcess (FspHobListPtr);

  //
  // FspHobList is not complete at this moment.
  // Save FspHobList pointer to hob, so that it can be got later
  //
  HobData = BuildGuidHob (
             &gFspHobGuid,
             sizeof (VOID *)
             );
  ASSERT (HobData != NULL);
  CopyMem (HobData, &FspHobListPtr, sizeof (FspHobListPtr));

  return Status;
}

/**
  Do FSP initialization.

  @return FSP initialization status.
**/
EFI_STATUS
EFIAPI
FspmWrapperInit (
  VOID
  )
{
  EFI_STATUS           Status;

  Status = PeiFspMemoryInit ();
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  This is the entrypoint of PEIM

  @param[in] FileHandle  Handle of the file being invoked.
  @param[in] PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS if it completed successfully.
**/
EFI_STATUS
EFIAPI
FspmWrapperPeimEntryPoint (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  DEBUG((DEBUG_INFO, "FspmWrapperPeimEntryPoint\n"));

  FspmWrapperInit ();

  return EFI_SUCCESS;
}

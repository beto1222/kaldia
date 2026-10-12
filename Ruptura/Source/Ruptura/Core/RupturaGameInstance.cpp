#include "Core/RupturaGameInstance.h"
#include "Kismet/GameplayStatics.h"

URupturaGameInstance* URupturaGameInstance::Get(const UObject* WorldContext)
{
	return Cast<URupturaGameInstance>(UGameplayStatics::GetGameInstance(WorldContext));
}

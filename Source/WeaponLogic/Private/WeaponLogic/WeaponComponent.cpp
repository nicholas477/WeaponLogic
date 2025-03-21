// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponLogic/WeaponComponent.h"

#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

UE_DEFINE_GAMEPLAY_TAG(UWeaponComponent::TAG_WeaponState_Idle,		"WeaponLogic.WeaponState.Idle");
UE_DEFINE_GAMEPLAY_TAG(UWeaponComponent::TAG_WeaponState_Firing,	"WeaponLogic.WeaponState.Firing");
UE_DEFINE_GAMEPLAY_TAG(UWeaponComponent::TAG_WeaponState_Reloading,	"WeaponLogic.WeaponState.Reloading");

UWeaponComponent::UWeaponComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	PrimaryComponentTick.bCanEverTick = true;

	WeaponState = TAG_WeaponState_Idle;
	NextFiringTime = -UE_BIG_NUMBER;
	StartFiringTime = -UE_BIG_NUMBER;
	bWantsFire = false;
	bMulticastFire = true;
}

void UWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWeaponComponent, WeaponState);
}

void UWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner()->GetLocalRole() == ENetRole::ROLE_SimulatedProxy)
	{
		return;
	}

	if (bWantsFire)
	{
		if (CanStartFiring())
		{
			StartFiringTime = GetTime();
			SetWeaponState(TAG_WeaponState_Firing);
		}
	}
	else
	{
		if (WeaponState == TAG_WeaponState_Firing)
		{
			SetWeaponState(TAG_WeaponState_Idle);
		}
	}

	if (WeaponState == TAG_WeaponState_Firing)
	{
		check(WeaponConfig.TimeBetweenShots > 0.f);
		if (WeaponConfig.TimeBetweenShots <= 0.f)
		{
			return;
		}

		// We can shoot if we have waited long enough between shots
		while (NextFiringTime <= GetTime())
		{
			// The next firing time is whichever happened sooner:
			// 
			// - The time we started holding down the fire key
			// - or the next time we are allowed to fire
			const float FiringTime = FMath::Max(StartFiringTime, NextFiringTime);
			Fire(FiringTime);

			NextFiringTime = FiringTime + WeaponConfig.TimeBetweenShots;
		}
	}
}

void UWeaponComponent::InputStartFire()
{
	if (GetOwner()->GetLocalRole() == ENetRole::ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (!GetOwner()->HasAuthority())
	{
		Server_InputStartFire();
	}

	bWantsFire = true;
}

void UWeaponComponent::Server_InputStartFire_Implementation()
{
	InputStartFire();
}

void UWeaponComponent::InputStopFire()
{
	if (GetOwner()->GetLocalRole() == ENetRole::ROLE_SimulatedProxy)
	{
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		Server_InputStopFire();
	}

	bWantsFire = false;
}

void UWeaponComponent::Server_InputStopFire_Implementation()
{
	InputStopFire();
}

bool UWeaponComponent::CanStartFiring_Implementation() const
{
	return WeaponState == TAG_WeaponState_Idle;
}

void UWeaponComponent::SetWeaponState(FGameplayTag NewState)
{
	// Only allow setting the weapon state on local owner or auth
	if (GetOwner()->GetLocalRole() == ENetRole::ROLE_SimulatedProxy)
	{
		return;
	}

	if (NewState == WeaponState)
	{
		return;
	}

	FGameplayTag OldState = WeaponState;
	WeaponState = NewState;
	OnRep_WeaponState(OldState);
}

void UWeaponComponent::OnRep_WeaponState(FGameplayTag OldState)
{
	OnWeaponStateChange.Broadcast(this, OldState, WeaponState);
}

void UWeaponComponent::Fire(float Time)
{
	if (GetOwner()->HasAuthority())
	{
		if (bMulticastFire)
		{
			Multicast_Fire(Time);
		}
		else
		{
			OnWeaponFire.Broadcast(this, Time, true);
		}
	}
	else if (GetOwner()->GetLocalRole() == ENetRole::ROLE_AutonomousProxy)
	{
		OnWeaponFire.Broadcast(this, Time, false);
	}
}

float UWeaponComponent::GetTime() const
{
	if (GetWorld()->GetGameState())
	{
		return GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	}
	return 0.0f;
}

void UWeaponComponent::Multicast_Fire_Implementation(float Time)
{
	OnWeaponFire.Broadcast(this, Time, true);
}


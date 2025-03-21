// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WeaponComponent.generated.h"

class UWeaponComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponStateChange, UWeaponComponent*, WeaponComponent, FGameplayTag, OldState, FGameplayTag, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponFire, UWeaponComponent*, WeaponComponent, float, Time, bool, bFiredFromAuthority);

USTRUCT(BlueprintType)
struct FWeaponConfig
{
	GENERATED_BODY()

	// The time between consecutive shots. This is the inverse of RPM (rounds per minute)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapon Config", meta=(Units="Seconds"))
	float TimeBetweenShots;

	/** defaults */
	FWeaponConfig()
	{
		TimeBetweenShots = 1.f;
	}
};

/**
 * Handles the state machine/timing for weapon firing and consuming ammo. Only handles replicated logic, no visuals. Bind to delegates for spawning projectiles.
 */
UCLASS( ClassGroup=(Weapon), meta = (BlueprintSpawnableComponent))
class WEAPONLOGIC_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UWeaponComponent(const FObjectInitializer& ObjectInitializer);

	static FNativeGameplayTag TAG_WeaponState_Idle;
	static FNativeGameplayTag TAG_WeaponState_Firing;
	static FNativeGameplayTag TAG_WeaponState_Reloading;

	UPROPERTY(BlueprintAssignable)
	FOnWeaponStateChange OnWeaponStateChange;

	// Spawn your projectiles or bullets here. Note: this event may fire on client and server depending on 
	UPROPERTY(BlueprintAssignable)
	FOnWeaponFire OnWeaponFire;

	UFUNCTION(BlueprintPure, Category = "Weapon Component")
	FGameplayTag GetWeaponState() const { return WeaponState; }

	UFUNCTION(BlueprintPure, Category = "Weapon Component")
	FWeaponConfig GetWeaponConfig() const { return WeaponConfig; }

	// Call this from player/AI input to start firing.
	UFUNCTION(BlueprintCallable, Category = "Weapon Component")
	void InputStartFire();

	// Call this from player/AI input to stop firing.
	UFUNCTION(BlueprintCallable, Category = "Weapon Component")
	void InputStopFire();

	// If true, then we can transition into the Firing weapon state
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Weapon Component")
	bool CanStartFiring() const;

protected:
	// Only changes if this (component is NOT a simulated proxy) && (NewState != WeaponState)
	void SetWeaponState(FGameplayTag NewState);

	UPROPERTY(ReplicatedUsing=OnRep_WeaponState, VisibleAnywhere, Category = "Weapon Component")
	FGameplayTag WeaponState;

	UFUNCTION()
	void OnRep_WeaponState(FGameplayTag OldState);

	// The next time a round may be fired. This is compared to server time to see if we can fire another shot
	UPROPERTY(VisibleInstanceOnly, Category = "Weapon Component")
	float NextFiringTime;

	// The time that the weapon state changed to Firing
	UPROPERTY(VisibleInstanceOnly, Category = "Weapon Component")
	float StartFiringTime;

	// If true, the user is holding down the "fire" key
	UPROPERTY(VisibleInstanceOnly, Category = "Weapon Component")
	bool bWantsFire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Component")
	FWeaponConfig WeaponConfig;

	void Fire(float Time);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Fire(float Time);

	// Broadcast OnWeaponFire as a multicast (client + server) event.
	// If false, OnWeaponFire is only broadcasted on server
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapon Component")
	bool bMulticastFire;

	UFUNCTION(Server, Reliable)
	void Server_InputStartFire();

	UFUNCTION(Server, Reliable)
	void Server_InputStopFire();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Gets the server time from the game state
	float GetTime() const;
};

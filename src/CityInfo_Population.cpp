#include "CityInfo.h"

#include "Building.h"
#include "Calc.h"
#include "HousePopulation.h"
#include "PlayerMessage.h"
#include "Util.h"

#include "Data/Building.h"
#include "Data/CityInfo.h"
#include "Data/Model.h"
#include "Data/Random.h"
#include "Data/Settings.h"
#include "Data/Tutorial.h"

static void addPeopleToCensus(int numPeople);
static void removePeopleFromCensus(int numPeople);
static void removePeopleFromCensusInDecennium(int decennium, int numPeople);
static int getPeopleInAgeDecennium(int decennium);
static void recalculatePopulation();
static void healthCauseDisease(int totalPeople);

static const int yearlyBirthsPerDecennium[10] = {
	0, 3, 16, 9, 2, 0, 0, 0, 0, 0
};

static const int yearlyDeathsPerHealthPerDecennium[11][10] = {
	{20, 10, 5, 10, 20, 30, 50, 85, 100, 100},
	{15, 8, 4, 8, 16, 25, 45, 70, 90, 100},
	{10, 6, 2, 6, 12, 20, 30, 55, 80, 90},
	{5, 4, 0, 4, 8, 15, 25, 40, 65, 80},
	{3, 2, 0, 2, 6, 12, 20, 30, 50, 70},
	{2, 0, 0, 0, 4, 8, 15, 25, 40, 60},
	{1, 0, 0, 0, 2, 6, 12, 20, 30, 50},
	{0, 0, 0, 0, 0, 4, 8, 15, 20, 40},
	{0, 0, 0, 0, 0, 2, 6, 10, 15, 30},
	{0, 0, 0, 0, 0, 0, 4, 5, 10, 20},
	{0, 0, 0, 0, 0, 0, 0, 2, 5, 10}
};

static const int sentimentPerTaxRate[26] = {
	3, 2, 2, 2, 1, 1, 1, 0, 0, -1,
	-2, -2, -3, -3, -3, -5, -5, -5, -5, -6,
	-6, -6, -6, -6, -6, -6
};

void CityInfo_Population_recordMonthlyPopulation()
{
	Data_CityInfo.monthlyPopulation[Data_CityInfo.monthlyPopulationNextIndex++] = Data_CityInfo.population;
	if (Data_CityInfo.monthlyPopulationNextIndex >= 2400) {
		Data_CityInfo.monthlyPopulationNextIndex = 0;
	}
	++Data_CityInfo.monthsSinceStart;
}

void CityInfo_Population_changeHappiness(int amount)
{
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse && Data_Buildings[i].houseSize) {
			Data_Buildings[i].sentiment.houseHappiness += amount;
			BOUND(Data_Buildings[i].sentiment.houseHappiness, 0, 100);
		}
	}
}

void CityInfo_Population_setMaxHappiness(int max)
{
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse && Data_Buildings[i].houseSize) {
			if (Data_Buildings[i].sentiment.houseHappiness > max) {
				Data_Buildings[i].sentiment.houseHappiness = max;
			}
			BOUND(Data_Buildings[i].sentiment.houseHappiness, 0, 100);
		}
	}
}

static int getSentimentPenaltyForTentDwellers()
{
	int penalty;
	int pctTents = Calc_getPercentage(
		Data_CityInfo.populationPeopleInTents, Data_CityInfo.population);
	if (Data_CityInfo.populationPeopleInVillasPalaces > 0) {
		if (pctTents > 56) {
			penalty = 0;
		} else if (pctTents >= 40) {
			penalty = -3;
		} else if (pctTents >= 26) {
			penalty = -4;
		} else if (pctTents >= 10) {
			penalty = -5;
		} else {
			penalty = -6;
		}
	} else if (Data_CityInfo.populationPeopleInLargeInsulaAndAbove) {
		if (pctTents > 56) {
			penalty = 0;
		} else if (pctTents >= 40) {
			penalty = -2;
		} else if (pctTents >= 26) {
			penalty = -3;
		} else if (pctTents >= 10) {
			penalty = -4;
		} else {
			penalty = -5;
		}
	} else {
		if (pctTents >= 40) {
			penalty = 0;
		} else if (pctTents >= 26) {
			penalty = -1;
		} else if (pctTents >= 10) {
			penalty = -2;
		} else {
			penalty = -3;
		}
	}
	return penalty;
}

void CityInfo_Population_calculateSentiment()
{
	int peopleInHouses = HousePopulation_calculatePeoplePerType();
	if (peopleInHouses < Data_CityInfo.population) {
		removePeopleFromCensus(Data_CityInfo.population - peopleInHouses);
	}

	int sentimentPenaltyTents;
	int sentimentContributionEmployment = 0;
	int sentimentContributionWages = 0;
	int sentimentContributionTaxes = sentimentPerTaxRate[Data_CityInfo.taxPercentage];

	// wages contribution
	int wageDiff = Data_CityInfo.wages - Data_CityInfo.wagesRome;
	if (wageDiff < 0) {
		sentimentContributionWages = wageDiff / 2;
		if (!sentimentContributionWages) {
			sentimentContributionWages = -1;
		}
	} else if (wageDiff > 7) {
		sentimentContributionWages = 4;
	} else if (wageDiff > 4) {
		sentimentContributionWages = 3;
	} else if (wageDiff > 1) {
		sentimentContributionWages = 2;
	} else if (wageDiff > 0) {
		sentimentContributionWages = 1;
	}
	Data_CityInfo.populationSentimentWages = Data_CityInfo.wages;
	// unemployment contribution
	if (Data_CityInfo.unemploymentPercentage > 25) {
		sentimentContributionEmployment = -3;
	} else if (Data_CityInfo.unemploymentPercentage > 17) {
		sentimentContributionEmployment = -2;
	} else if (Data_CityInfo.unemploymentPercentage > 10) {
		sentimentContributionEmployment = -1;
	} else if (Data_CityInfo.unemploymentPercentage > 4) {
		sentimentContributionEmployment = 0;
	} else {
		sentimentContributionEmployment = 1;
	}
	// tent contribution
	if (Data_CityInfo.populationSentimentIncludeTents) {
		sentimentPenaltyTents = getSentimentPenaltyForTentDwellers();
		Data_CityInfo.populationSentimentIncludeTents = 0;
	} else {
		sentimentPenaltyTents = 0;
		Data_CityInfo.populationSentimentIncludeTents = 1;
	}

	int housesCalculated = 0;
	int housesNeedingFood = 0;
	int totalSentimentContributionFood = 0;
	int totalSentimentPenaltyTents = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse != 1 || !b->houseSize) {
			continue;
		}
		if (!b->housePopulation) {
			b->sentiment.houseHappiness =
				10 + Data_Model_Difficulty.sentiment[Data_Settings.difficulty];
			continue;
		}
		if (Data_CityInfo.population < 300) {
			// small town has no complaints
			sentimentContributionEmployment = 0;
			sentimentContributionTaxes = 0;
			sentimentContributionWages = 0;

			b->sentiment.houseHappiness = Data_Model_Difficulty.sentiment[Data_Settings.difficulty];
			if (Data_CityInfo.population < 200) {
				b->sentiment.houseHappiness += 10;
			}
			continue;
		}
		// population >= 300
		housesCalculated++;
		int sentimentContributionFood = 0;
		int sentimentContributionTents = 0;
		if (!Data_Model_Houses[b->subtype.houseLevel].foodTypes) {
			// tents
			b->houseDaysWithoutFood = 0;
			sentimentContributionTents = sentimentPenaltyTents;
			totalSentimentPenaltyTents += sentimentPenaltyTents;
		} else {
			// shack+
			housesNeedingFood++;
			if (b->data.house.numFoods >= 2) {
				sentimentContributionFood = 2;
				totalSentimentContributionFood += 2;
				b->houseDaysWithoutFood = 0;
			} else if (b->data.house.numFoods > 0) {
				sentimentContributionFood = 1;
				totalSentimentContributionFood += 1;
				b->houseDaysWithoutFood = 0;
			} else {
				// needs food but has no food
				if (b->houseDaysWithoutFood < 3) {
					b->houseDaysWithoutFood++;
				}
				sentimentContributionFood = -b->houseDaysWithoutFood;
				totalSentimentContributionFood -= b->houseDaysWithoutFood;
			}
		}
		b->sentiment.houseHappiness += sentimentContributionTaxes;
		b->sentiment.houseHappiness += sentimentContributionWages;
		b->sentiment.houseHappiness += sentimentContributionEmployment;
		b->sentiment.houseHappiness += sentimentContributionFood;
		b->sentiment.houseHappiness += sentimentContributionTents;
		BOUND(b->sentiment.houseHappiness, 0, 100);
	}

	int sentimentContributionFood = 0;
	int sentimentContributionTents = 0;
	if (housesNeedingFood) {
		sentimentContributionFood = totalSentimentContributionFood / housesNeedingFood;
	}
	if (housesCalculated) {
		sentimentContributionTents = totalSentimentPenaltyTents / housesCalculated;
	}

	int totalSentiment = 0;
	int totalHouses = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse == 1 && Data_Buildings[i].houseSize &&
			Data_Buildings[i].housePopulation) {
			totalHouses++;
			totalSentiment += Data_Buildings[i].sentiment.houseHappiness;
		}
	}
	if (totalHouses) {
		Data_CityInfo.citySentiment = totalSentiment / totalHouses;
	} else {
		Data_CityInfo.citySentiment = 60;
	}
	if (Data_CityInfo.citySentimentChangeMessageDelay) {
		Data_CityInfo.citySentimentChangeMessageDelay--;
	}
	if (Data_CityInfo.citySentiment < 48 && Data_CityInfo.citySentiment < Data_CityInfo.citySentimentLastTime) {
		if (Data_CityInfo.citySentimentChangeMessageDelay <= 0) {
			Data_CityInfo.citySentimentChangeMessageDelay = 3;
			if (Data_CityInfo.citySentiment < 35) {
				PlayerMessage_post(0, 48, 0, 0);
			} else if (Data_CityInfo.citySentiment < 40) {
				PlayerMessage_post(0, 47, 0, 0);
			} else {
				PlayerMessage_post(0, 46, 0, 0);
			}
		}
	}

	int worstSentiment = 0;
	Data_CityInfo.populationEmigrationCause = 0;
	if (sentimentContributionFood < worstSentiment) {
		worstSentiment = sentimentContributionFood;
		Data_CityInfo.populationEmigrationCause = 1;
	}
	if (sentimentContributionEmployment < worstSentiment) {
		worstSentiment = sentimentContributionEmployment;
		Data_CityInfo.populationEmigrationCause = 2;
	}
	if (sentimentContributionTaxes < worstSentiment) {
		worstSentiment = sentimentContributionTaxes;
		Data_CityInfo.populationEmigrationCause = 3;
	}
	if (sentimentContributionWages < worstSentiment) {
		worstSentiment = sentimentContributionWages;
		Data_CityInfo.populationEmigrationCause = 4;
	}
	if (sentimentContributionTents < worstSentiment) {
		worstSentiment = sentimentContributionTents;
		Data_CityInfo.populationEmigrationCause = 5;
	}
	Data_CityInfo.citySentimentLastTime = Data_CityInfo.citySentiment;
}

void CityInfo_Population_calculateMigrationSentiment()
{
	if (Data_CityInfo.citySentiment > 70) {
		Data_CityInfo.populationMigrationPercentage = 100;
	} else if (Data_CityInfo.citySentiment > 60) {
		Data_CityInfo.populationMigrationPercentage = 75;
	} else if (Data_CityInfo.citySentiment >= 50) {
		Data_CityInfo.populationMigrationPercentage = 50;
	} else if (Data_CityInfo.citySentiment > 40) {
		Data_CityInfo.populationMigrationPercentage = 0;
	} else if (Data_CityInfo.citySentiment > 30) {
		Data_CityInfo.populationMigrationPercentage = -10;
	} else if (Data_CityInfo.citySentiment > 20) {
		Data_CityInfo.populationMigrationPercentage = -25;
	} else {
		Data_CityInfo.populationMigrationPercentage = -50;
	}

	Data_CityInfo.populationImmigrationAmountPerBatch = 0;
	Data_CityInfo.populationEmigrationAmountPerBatch = 0;

	int populationCap = 200000;
	if (IsTutorial1()) {
		if (!Data_Tutorial.tutorial1.fire ||
			!Data_Tutorial.tutorial1.collapse ||
			!Data_Tutorial.tutorial1.senateBuilt) {
			populationCap = 80;
		}
	} else if (IsTutorial2()) {
		if (!Data_Tutorial.tutorial2.granaryBuilt) {
			populationCap = 150;
		} else if (!Data_Tutorial.tutorial2.potteryMade) {
			populationCap = 520;
		}
	}
	if (Data_CityInfo.population >= populationCap) {
		Data_CityInfo.populationMigrationPercentage = 0;
		return;
	}
	// war scares immigrants away
	if (Data_CityInfo.numEnemiesInCity + Data_CityInfo.numImperialSoldiersInCity > 3 &&
		Data_CityInfo.populationMigrationPercentage > 0) {
		Data_CityInfo.populationMigrationPercentage = 0;
		return;
	}
	if (Data_CityInfo.populationMigrationPercentage > 0) {
		// immigration
		if (Data_CityInfo.populationEmigrationDuration) {
			Data_CityInfo.populationEmigrationDuration--;
		} else {
			Data_CityInfo.populationImmigrationAmountPerBatch =
				Calc_adjustWithPercentage(12, Data_CityInfo.populationMigrationPercentage);
			Data_CityInfo.populationImmigrationDuration = 2;
		}
	} else if (Data_CityInfo.populationMigrationPercentage < 0) {
		// emigration
		if (Data_CityInfo.populationImmigrationDuration) {
			Data_CityInfo.populationImmigrationDuration--;
		} else if (Data_CityInfo.population > 100) {
			Data_CityInfo.populationEmigrationAmountPerBatch =
				Calc_adjustWithPercentage(12, -Data_CityInfo.populationMigrationPercentage);
			Data_CityInfo.populationEmigrationDuration = 2;
		}
	}
}

void CityInfo_Population_updateHealthRate()
{
	if (Data_CityInfo.population < 200 || IsTutorial1() || IsTutorial2()) {
		Data_CityInfo.healthRate = 50;
		Data_CityInfo.healthRateTarget = 50;
		return;
	}
	int totalPopulation = 0;
	int healthyPopulation = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse != 1 || !b->houseSize || !b->housePopulation) {
			continue;
		}
		totalPopulation += b->housePopulation;
		if (b->subtype.houseLevel <= HouseLevel_LargeTent) {
			if (b->data.house.clinic) {
				healthyPopulation += b->housePopulation;
			} else {
				healthyPopulation += b->housePopulation / 4;
			}
		} else if (b->data.house.clinic) {
			if (b->houseDaysWithoutFood <= 0) {
				healthyPopulation += b->housePopulation;
			} else {
				healthyPopulation += b->housePopulation / 4;
			}
		} else if (b->houseDaysWithoutFood <= 0) {
			healthyPopulation += b->housePopulation / 4;
		}
	}
	Data_CityInfo.healthRateTarget = Calc_getPercentage(healthyPopulation, totalPopulation);
	if (Data_CityInfo.healthRate < Data_CityInfo.healthRateTarget) {
		Data_CityInfo.healthRate += 2;
		if (Data_CityInfo.healthRate > Data_CityInfo.healthRateTarget) {
			Data_CityInfo.healthRate = Data_CityInfo.healthRateTarget;
		}
	} else if (Data_CityInfo.healthRate > Data_CityInfo.healthRateTarget) {
		Data_CityInfo.healthRate -= 2;
		if (Data_CityInfo.healthRate > Data_CityInfo.healthRateTarget) {
			Data_CityInfo.healthRate = Data_CityInfo.healthRateTarget;
		}
	}
	BOUND(Data_CityInfo.healthRate, 0, 100);

	healthCauseDisease(totalPopulation);
}

static void healthCauseDisease(int totalPeople)
{
	if (Data_CityInfo.healthRate >= 40) {
		return;
	}
	int chanceValue = Data_Random.random1_7bit & 0x3f;
	if (Data_CityInfo.godCurseVenusActive) {
		// force plague
		chanceValue = 0;
		Data_CityInfo.godCurseVenusActive = 0;
	}
	if (chanceValue > 40 - Data_CityInfo.healthRate) {
		return;
	}

	int sickPeople = Calc_adjustWithPercentage(totalPeople, 7 + (Data_Random.random1_7bit & 3));
	if (sickPeople <= 0) {
		return;
	}
	CityInfo_Population_changeHealthRate(10);
	int peopleToKill = sickPeople - Data_CityInfo.numHospitalWorkers;
	if (peopleToKill <= 0) {
		PlayerMessage_post(1, 102, 0, 0);
		return;
	}
	if (Data_CityInfo.numHospitalWorkers > 0) {
		PlayerMessage_post(1, 103, 0, 0);
	} else {
		PlayerMessage_post(1, 104, 0, 0);
	}
	Data_Tutorial.tutorial3.disease = 1;
	// kill people who don't have access to a doctor
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 1 && b->houseSize && b->housePopulation) {
			if (!b->data.house.clinic) {
				peopleToKill -= b->housePopulation;
				Building_collapseOnFire(i, 1);
				if (peopleToKill <= 0) {
					return;
				}
			}
		}
	}
	// kill people in tents
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 1 && b->houseSize && b->housePopulation) {
			if (b->subtype.houseLevel <= HouseLevel_LargeTent) {
				peopleToKill -= b->housePopulation;
				Building_collapseOnFire(i, 1);
				if (peopleToKill <= 0) {
					return;
				}
			}
		}
	}
	// kill anyone
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 1 && b->houseSize && b->housePopulation) {
			peopleToKill -= b->housePopulation;
			Building_collapseOnFire(i, 1);
			if (peopleToKill <= 0) {
				return;
			}
		}
	}
}

void CityInfo_Population_changeHealthRate(int amount)
{
	Data_CityInfo.healthRate += amount;
	BOUND(Data_CityInfo.healthRate, 0, 100);
}

static void recalculatePopulation()
{
	Data_CityInfo.population = 0;
	for (int i = 0; i < 100; i++) {
		Data_CityInfo.population += Data_CityInfo.populationPerAge[i];
	}
	if (Data_CityInfo.population > Data_CityInfo.populationHighestEver) {
		Data_CityInfo.populationHighestEver = Data_CityInfo.population;
	}
}

static int getPeopleInAgeDecennium(int decennium)
{
	int pop = 0;
	for (int i = 0; i < 10; i++) {
		pop += Data_CityInfo.populationPerAge[10 * decennium + i];
	}
	return pop;
}

static void removePeopleFromCensusInDecennium(int decennium, int numPeople)
{
	int emptyBuckets = 0;
	int age = 0;
	while (numPeople > 0 && emptyBuckets < 10) {
		if (Data_CityInfo.populationPerAge[10 * decennium + age] <= 0) {
			emptyBuckets++;
		} else {
			Data_CityInfo.populationPerAge[10 * decennium + age]--;
			numPeople--;
			emptyBuckets = 0;
		}
		age++;
		if (age >= 10) {
			age = 0;
		}
	}
}

static void removePeopleFromCensus(int numPeople)
{
	int index = Data_Random.poolIndex;
	int emptyBuckets = 0;
	// remove people randomly up to age 63
	while (numPeople > 0 && emptyBuckets < 100) {
		int age = Data_Random.pool[index] & 0x3f;
		if (Data_CityInfo.populationPerAge[age] <= 0) {
			emptyBuckets++;
		} else {
			Data_CityInfo.populationPerAge[age]--;
			numPeople--;
			emptyBuckets = 0;
		}
		if (++index >= 100) {
			index = 0;
		}
	}
	// if random didn't work: remove from age 10 and up
	emptyBuckets = 0;
	int age = 10;
	while (numPeople > 0 && emptyBuckets < 100) {
		if (Data_CityInfo.populationPerAge[age] <= 0) {
			emptyBuckets++;
		} else {
			Data_CityInfo.populationPerAge[age]--;
			numPeople--;
			emptyBuckets = 0;
		}
		if (++age >= 100) {
			index = 0;
		}
	}
}

static void addPeopleToCensus(int numPeople)
{
	int odd = 0;
	int index = Data_Random.poolIndex;
	for (int i = 0; i < numPeople; i++, index++, odd = 1 - odd) {
		if (index >= 100) {
			index = 0;
		}
		int age = Data_Random.pool[index] & 0x3f; // 63
		if (age > 50) {
			age -= 30;
		} else if (age < 10 && odd) {
			age += 20;
		}
		Data_CityInfo.populationPerAge[age]++;
	}
}

void CityInfo_Population_addPeople(int numPeople)
{
	Data_CityInfo.populationLastChange = numPeople;
	if (numPeople > 0) {
		addPeopleToCensus(numPeople);
	} else if (numPeople < 0) {
		removePeopleFromCensus(numPeople);
	}
	recalculatePopulation();
}

void CityInfo_Population_removePeopleHomeRemoved(int numPeople)
{
	Data_CityInfo.populationLostInRemoval += numPeople;
	removePeopleFromCensus(numPeople);
	recalculatePopulation();
}

void CityInfo_Population_addPeopleHomeless(int numPeople)
{
	Data_CityInfo.populationLostHomeless -= numPeople;
	addPeopleToCensus(numPeople);
	recalculatePopulation();
}

void CityInfo_Population_removePeopleHomeless(int numPeople)
{
	Data_CityInfo.populationLostHomeless += numPeople;
	removePeopleFromCensus(numPeople);
	recalculatePopulation();
}

void CityInfo_Population_removePeopleForTroopRequest(int amount)
{
	int removed = HousePopulation_removePeople(amount);
	removePeopleFromCensus(removed);
	Data_CityInfo.populationLostTroopRequest += amount;
	recalculatePopulation();
}

int CityInfo_Population_getPeopleOfWorkingAge()
{
	return
		getPeopleInAgeDecennium(2) +
		getPeopleInAgeDecennium(3) +
		getPeopleInAgeDecennium(4);
}

int CityInfo_Population_getNumberOfSchoolAgeChildren()
{
	int pop = 0;
	for (int i = 0; i < 14; i++) {
		pop += Data_CityInfo.populationPerAge[i];
	}
	return pop;
}

int CityInfo_Population_getNumberOfAcademyChildren()
{
	int pop = 0;
	for (int i = 14; i < 21; i++) {
		pop += Data_CityInfo.populationPerAge[i];
	}
	return pop;
}

static void yearlyAdvanceAgesAndCalculateDeaths()
{
	int aged100 = Data_CityInfo.populationPerAge[99];
	for (int age = 99; age > 0; age--) {
		Data_CityInfo.populationPerAge[age] = Data_CityInfo.populationPerAge[age-1];
	}
	Data_CityInfo.populationPerAge[0] = 0;
	Data_CityInfo.populationYearlyDeaths = 0;
	for (int decennium = 9; decennium >= 0; decennium--) {
		int people = getPeopleInAgeDecennium(decennium);
		int deaths = Calc_adjustWithPercentage(people,
			yearlyDeathsPerHealthPerDecennium[Data_CityInfo.healthRate / 10][decennium]);
		int removed = HousePopulation_removePeople(deaths + aged100);
		removePeopleFromCensusInDecennium(decennium, removed);
		// ^ BUGFIX should be deaths only, now aged100 are removed from census while they weren't *in* the census
		Data_CityInfo.populationYearlyDeaths += removed;
		aged100 = 0;
	}
}

static void yearlyCalculateBirths()
{
	Data_CityInfo.populationYearlyBirths = 0;
	for (int decennium = 9; decennium >= 0; decennium--) {
		int people = getPeopleInAgeDecennium(decennium);
		int births = Calc_adjustWithPercentage(people, yearlyBirthsPerDecennium[decennium]);
		int added = HousePopulation_addPeople(births);
		Data_CityInfo.populationPerAge[0] += added;
		Data_CityInfo.populationYearlyBirths += added;
	}
}

static void yearlyUpdateAfterDeathsBirths()
{
	Data_CityInfo.populationYearlyUpdatedNeeded = 0;
	Data_CityInfo.populationLastYear = Data_CityInfo.population;
	recalculatePopulation();

	Data_CityInfo.populationLostInRemoval = 0;
	Data_CityInfo.populationTotalAllYears += Data_CityInfo.population;
	Data_CityInfo.populationTotalYears++;
	Data_CityInfo.populationAveragePerYear = Data_CityInfo.populationTotalAllYears / Data_CityInfo.populationTotalYears;
}

void CityInfo_Population_requestYearlyUpdate()
{
	Data_CityInfo.populationYearlyUpdatedNeeded = 1;
	HousePopulation_calculatePeoplePerType();
}

void CityInfo_Population_yearlyUpdate()
{
	if (Data_CityInfo.populationYearlyUpdatedNeeded) {
		yearlyAdvanceAgesAndCalculateDeaths();
		yearlyCalculateBirths();
		yearlyUpdateAfterDeathsBirths();
	}
}

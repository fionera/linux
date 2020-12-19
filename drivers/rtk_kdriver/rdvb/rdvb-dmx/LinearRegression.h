#ifndef LINEARREGRESSION_H
#define LINEARREGRESSION_H

#define LR_SAMPLE_NUM 200
#define GROUP_SAMPLE_NUM 100
#define SUM_GROUP_NUM (LR_SAMPLE_NUM/GROUP_SAMPLE_NUM)



typedef struct
{
    int64_t x;  /* stc */
    int64_t y;  /* pcr */
} LINEARPAIR;

/*
 * m_a: Slope
 * m_b: y-intercept, is a point where the graph of a function or relation
  *     intersects with the y-axis of the coordinate system. 
*/
typedef struct LINEARREGRESSION_T_ {
	int64_t m_a;
	int64_t m_b;

	int64_t m_pairNum;
	int m_pairIdx;

	LINEARPAIR m_dataPair[LR_SAMPLE_NUM];
	LINEARPAIR m_BasePair;
	LINEARPAIR m_restoredPair;

} LINEARREGRESSION_T;


void LRegress_Reset(LINEARREGRESSION_T *pLRegress );
int LRegress_InsertDataPair(LINEARREGRESSION_T *pLRegress , LINEARPAIR pair);
int64_t LRegress_GetPredictY(LINEARREGRESSION_T *pLRegress , int64_t x);


void LRegress_PopDataPair(LINEARREGRESSION_T *pLRegress);
void LRegress_dumpDatabase(LINEARREGRESSION_T *pLRegress, const char * caller, const int callLine);




#endif /*LINEARREGRESSION_H*/


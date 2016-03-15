// Gamma distribution with natural log link function ( mean = std::exp(prediction) )
// -19 <= prediction <= +19

#include "gamma.h"
#include <math.h>
#include <iostream>

CGamma::CGamma()
{
}

CGamma::~CGamma()
{
}


void CGamma::ComputeWorkingResponse
(
    const double *adY,
    const double *adMisc,
    const double *adOffset,
    const double *adF, 
    double *adZ, 
    const double *adWeight,
    const bag& afInBag,
    unsigned long nTrain
)
{
  unsigned long i = 0;
  double dF = 0.0;
  
  if (!(adY && adF && adZ && adWeight)) {
    throw GBM::invalid_argument();
  }

  for(i=0; i<nTrain; i++)
    {
      dF = adF[i] + ((adOffset==NULL) ? 0.0 : adOffset[i]);
      adZ[i] = adY[i]*std::exp(-dF)-1.0;
    }
  
}


void CGamma::InitF
(
    const double *adY,
    const double *adMisc,
    const double *adOffset, 
    const double *adWeight,
    double &dInitF, 
    unsigned long cLength
)
{
    double dSum=0.0;
    double dTotalWeight = 0.0;
    double Min = -19.0;
    double Max = +19.0;
    unsigned long i=0;

    if(adOffset==NULL)
    {
        for(i=0; i<cLength; i++)
        {
            dSum += adWeight[i]*adY[i];
            dTotalWeight += adWeight[i];
        }
    }
    else
    {
        for(i=0; i<cLength; i++)
        {
	  dSum += adWeight[i]*adY[i]*std::exp(-adOffset[i]);
	  dTotalWeight += adWeight[i];
        }
    }

    if (dSum <= 0.0) { dInitF = Min; }
    else { dInitF = std::log(dSum/dTotalWeight); }
    
    if (dInitF < Min) { dInitF = Min; }
    if (dInitF > Max) { dInitF = Max; }
}


double CGamma::Deviance
(
    const double *adY,
    const double *adMisc,
    const double *adOffset, 
    const double *adWeight,
    const double *adF,
    unsigned long cLength
)
{
  unsigned long i=0;
  double dL = 0.0;
  double dW = 0.0;
  double dF = 0.0;
  
  for(i=0; i!=cLength; i++)
    {
      dF = adF[i] + ((adOffset==NULL) ? 0.0 : adOffset[i]);
      dL += adWeight[i]*(adY[i]*std::exp(-dF) + dF);
      dW += adWeight[i];
    }
  
  return 2*dL/dW;
}


void CGamma::FitBestConstant
(
 const double *adY,
 const double *adMisc,
 const double *adOffset,
 const double *adW,
 const double *adF,
 double *adZ,
 const std::vector<unsigned long>& aiNodeAssign,
 unsigned long nTrain,
 VEC_P_NODETERMINAL vecpTermNodes,
 unsigned long cTermNodes,
 unsigned long cMinObsInNode,
 const bag& afInBag,
 const double *adFadj
)
{
 
  double dF = 0.0;
  unsigned long iObs = 0;
  unsigned long iNode = 0;
  double MaxVal = 19.0;
  double MinVal = -19.0;

  vecdNum.resize(cTermNodes);
  vecdNum.assign(vecdNum.size(),0.0);
  vecdDen.resize(cTermNodes);
  vecdDen.assign(vecdDen.size(),0.0);
  
  vecdMax.resize(cTermNodes);
  vecdMax.assign(vecdMax.size(),-HUGE_VAL);
  vecdMin.resize(cTermNodes);
  vecdMin.assign(vecdMin.size(),HUGE_VAL);
  
  for(iObs=0; iObs<nTrain; iObs++)
    {
      if(afInBag[iObs])
	{
	  dF = adF[iObs] + ((adOffset==NULL) ? 0.0 : adOffset[iObs]);
	  vecdNum[aiNodeAssign[iObs]] += adW[iObs]*adY[iObs]*std::exp(-dF);
	  vecdDen[aiNodeAssign[iObs]] += adW[iObs];
	  
	  // Keep track of largest and smallest prediction in each node
	  vecdMax[aiNodeAssign[iObs]] = R::fmax2(dF,vecdMax[aiNodeAssign[iObs]]);
	  vecdMin[aiNodeAssign[iObs]] = R::fmin2(dF,vecdMin[aiNodeAssign[iObs]]);
	}
    }
  
  for(iNode=0; iNode<cTermNodes; iNode++)
    {
      if(vecpTermNodes[iNode]!=NULL)
	{
	  if(vecdNum[iNode] == 0.0)
	    {
	      // Taken from poisson.cpp
	      
	      // DEBUG: if vecdNum==0 then prediction = -Inf
	      // Not sure what else to do except plug in an arbitrary
	      //   negative number, -1? -10? Let's use -19, then make
	      //   sure |adF| < 19 always.
	      vecpTermNodes[iNode]->dPrediction = MinVal;
	    }
	  
	  else if(vecdDen[iNode] == 0.0) { vecpTermNodes[iNode]->dPrediction = 0.0; }
	  
	  else { vecpTermNodes[iNode]->dPrediction = std::log(vecdNum[iNode]/vecdDen[iNode]); }
	  
	  if (vecdMax[iNode]+vecpTermNodes[iNode]->dPrediction > MaxVal)
	    { vecpTermNodes[iNode]->dPrediction = MaxVal - vecdMax[iNode]; }
	  if (vecdMin[iNode]+vecpTermNodes[iNode]->dPrediction < MinVal)
	    { vecpTermNodes[iNode]->dPrediction = MinVal - vecdMin[iNode]; }
	  
	}
    }
}

double CGamma::BagImprovement
(
	const double *adY,
	const double *adMisc,
	const double *adOffset,
	const double *adWeight,
	const double *adF,
	const double *adFadj,
	const bag& afInBag,
	double dStepSize,
	unsigned long nTrain
)
{
	double dReturnValue = 0.0;
	double dF = 0.0;
	double dW = 0.0;
	unsigned long i = 0;

	for(i=0; i<nTrain; i++)
	{
		if(!afInBag[i])
		{
			dF = adF[i] + ((adOffset==NULL) ? 0.0 : adOffset[i]);
			dReturnValue += adWeight[i]*(adY[i]*std::exp(-dF)*(1.0-exp(-dStepSize*adFadj[i])) - dStepSize*adFadj[i]);
			dW += adWeight[i];
		}
	}

	return 2*dReturnValue/dW;
}

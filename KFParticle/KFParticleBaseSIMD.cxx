//---------------------------------------------------------------------------------
// Implementation of the KFParticleBaseSIMD class
// .
// @author  I.Kisel, I.Kulakov, M.Zyzak
// @version 1.0
// @since   13.05.07
// 
// Class to reconstruct and store the decayed particle parameters.
// The method is described in CBM-SOFT note 2007-003, 
// ``Reconstruction of decayed particles based on the Kalman filter'', 
// http://www.gsi.de/documents/DOC-2007-May-14-1.pdf
//
// This class describes general mathematics which is used by KFParticle class
// 
//  -= Copyright &copy ALICE HLT and CBM L1 Groups =-
//_________________________________________________________________________________


#include "KFParticleBaseSIMD.h"

#include <iostream>
#include <iomanip>

static const float_v small = 1.e-20f;

KFParticleBaseSIMD::KFParticleBaseSIMD() :fQ(0.f), fNDF(-3), fChi2(0.f), fSFromDecay(0.f),
  SumDaughterMass(0.f), fMassHypo(-1), fId(-1), fAtProductionVertex(0), fIsVtxGuess(0),
  fIsVtxErrGuess(0), fIsLinearized(0), fPDG(0), fConstructMethod(0), fDaughterIds()
{ 
  //* Constructor 

  Initialize();
}

void KFParticleBaseSIMD::Initialize( const float_v Param[], const float_v Cov[], float_v Charge, float_v Mass )
{
  // Constructor from "cartesian" track, particle mass hypothesis should be provided
  //
  // Param[6] = { X, Y, Z, Px, Py, Pz } - position and momentum
  // Cov [21] = lower-triangular part of the covariance matrix:
  //
  //                (  0  .  .  .  .  . )
  //                (  1  2  .  .  .  . )
  //  Cov. matrix = (  3  4  5  .  .  . ) - numbering of covariance elements in Cov[]
  //                (  6  7  8  9  .  . )
  //                ( 10 11 12 13 14  . )
  //                ( 15 16 17 18 19 20 )


  for( Int_t i=0; i<6 ; i++ ) fP[i] = Param[i];
  for( Int_t i=0; i<21; i++ ) fC[i] = Cov[i];

  float_v energy = sqrt( Mass*Mass + fP[3]*fP[3] + fP[4]*fP[4] + fP[5]*fP[5]);
  fP[6] = energy;
  fP[7] = 0;
  fQ = Charge;
  fNDF = 0;
  fChi2 = 0;
  fAtProductionVertex = 0;
  fIsLinearized = 0;
  fSFromDecay = 0;

  float_v energyInv = 1.f/energy;
  float_v 
    h0 = fP[3]*energyInv,
    h1 = fP[4]*energyInv,
    h2 = fP[5]*energyInv;

  fC[21] = h0*fC[ 6] + h1*fC[10] + h2*fC[15];
  fC[22] = h0*fC[ 7] + h1*fC[11] + h2*fC[16];
  fC[23] = h0*fC[ 8] + h1*fC[12] + h2*fC[17];
  fC[24] = h0*fC[ 9] + h1*fC[13] + h2*fC[18];
  fC[25] = h0*fC[13] + h1*fC[14] + h2*fC[19];
  fC[26] = h0*fC[18] + h1*fC[19] + h2*fC[20];
  fC[27] = ( h0*h0*fC[ 9] + h1*h1*fC[14] + h2*h2*fC[20] 
	     + 2*(h0*h1*fC[13] + h0*h2*fC[18] + h1*h2*fC[19] ) );
  for( Int_t i=28; i<36; i++ ) fC[i] = 0.f;
  fC[35] = 1.f;

  SumDaughterMass = Mass;
  fMassHypo = Mass;
}

void KFParticleBaseSIMD::Initialize()
{
  //* Initialise covariance matrix and set current parameters to 0.0 

  for( Int_t i=0; i<8; i++) fP[i] = 0.f;
  for(Int_t i=0;i<36;++i) fC[i]=0.f;
  fC[0] = fC[2] = fC[5] = 100.f;
  fC[35] = 1.f;
  fNDF  = -3;
  fChi2 =  0.f;
  fQ = 0.f;
  fSFromDecay = 0.f;
  fAtProductionVertex = 0.f;
  fVtxGuess[0]=fVtxGuess[1]=fVtxGuess[2]=0.f;
  fVtxErrGuess[0]=fVtxErrGuess[1]=fVtxErrGuess[2]=0.f;
  fIsVtxGuess = 0;
  fIsVtxGuess = 0;
  fIsLinearized = 0;
  SumDaughterMass = 0.f;
  fMassHypo = -1;
}

void KFParticleBaseSIMD::SetVtxGuess( float_v x, float_v y, float_v z )
{
  //* Set decay vertex parameters for linearisation 

  fVtxGuess[0] = x;
  fVtxGuess[1] = y;
  fVtxGuess[2] = z;
  fIsLinearized = 1;
}

void KFParticleBaseSIMD::SetVtxErrGuess( float_v &dx, float_v &dy, float_v &dz )
{
  //* Set errors of the decay vertex parameters for linearisation 

  fVtxErrGuess[0] = dx;
  fVtxErrGuess[1] = dy;
  fVtxErrGuess[2] = dz;
  fIsVtxErrGuess = 1;
}

float_m KFParticleBaseSIMD::GetMomentum( float_v &p, float_v &error )  const 
{
  //* Calculate particle momentum

  float_v x = fP[3];
  float_v y = fP[4];
  float_v z = fP[5];
  float_v x2 = x*x;
  float_v y2 = y*y;
  float_v z2 = z*z;
  float_v p2 = x2+y2+z2;
  p = sqrt(p2);
  error = (x2*fC[9]+y2*fC[14]+z2*fC[20] + 2*(x*y*fC[13]+x*z*fC[18]+y*z*fC[19]) );
  const float_v LocalSmall = 1.e-4f;
  float_m mask = (0.f < error) && (LocalSmall < abs(p));
  error(!mask) = 1.e20f;
  error = sqrt(error);
  return (!mask);
}

float_m KFParticleBaseSIMD::GetPt( float_v &pt, float_v &error )  const 
{
  //* Calculate particle transverse momentum
  float_v px = fP[3];
  float_v py = fP[4];
  float_v px2 = px*px;
  float_v py2 = py*py;
  float_v pt2 = px2+py2;
  pt = sqrt(pt2);
  error = (px2*fC[9] + py2*fC[14] + 2*px*py*fC[13] );
  const float_v LocalSmall = 1.e-4f;
  float_m mask = ( (0.f < error) && (LocalSmall < abs(pt)));
  error(!mask) = 1.e20f;
  error = sqrt(error);
  return (!mask);
}

float_m KFParticleBaseSIMD::GetEta( float_v &eta, float_v &error )  const 
{
  //* Calculate particle pseudorapidity

  float_v ret = 0.f;

  const float_v BIG = 1.e10f;
  const float_v LocalSmall = 1.e-8f;

  float_v px = fP[3];
  float_v py = fP[4];
  float_v pz = fP[5];
  float_v pt2 = px*px + py*py;
  float_v p2 = pt2 + pz*pz;
  float_v p = sqrt(p2);
  float_v a = p + pz;
  float_v b = p - pz;
  eta = BIG;
  float_v c = 0.f;
  c(b > LocalSmall) = (a/b);
  float_v logc = 0.5f*KFPMath::Log(c);
  eta(LocalSmall<abs(c)) = logc;

  float_v h3 = -px*pz;
  float_v h4 = -py*pz;  
  float_v pt4 = pt2*pt2;
  float_v p2pt4 = p2*pt4;
  error = (h3*h3*fC[9] + h4*h4*fC[14] + pt4*fC[20] + 2*( h3*(h4*fC[13] + fC[18]*pt2) + pt2*h4*fC[19] ) );

  float_m mask = ((LocalSmall < abs(p2pt4)) && (0.f < error));
  error(mask) = sqrt(error/p2pt4);
  error(!mask) = BIG;

  return (!mask);
}

float_m KFParticleBaseSIMD::GetPhi( float_v &phi, float_v &error )  const 
{
  //* Calculate particle polar angle

  float_v px = fP[3];
  float_v py = fP[4];
  float_v px2 = px*px;
  float_v py2 = py*py;
  float_v pt2 = px2 + py2;
  phi = KFPMath::ATan2(py,px);
  error = (py2*fC[9] + px2*fC[14] - float_v(2.f)*px*py*fC[13] );

  float_m mask = (0.f < error) && (1.e-4f < pt2);
  error(mask) = sqrt(error)/pt2;
  error(!mask) = 1.e10f;
  return !mask;
}

float_m KFParticleBaseSIMD::GetR( float_v &r, float_v &error )  const 
{
  //* Calculate distance to the origin

  float_v x = fP[0];
  float_v y = fP[1];
  float_v x2 = x*x;
  float_v y2 = y*y;
  r = sqrt(x2 + y2);
  error = (x2*fC[0] + y2*fC[2] - float_v(2.f)*x*y*fC[1] );

  float_m mask = (0.f < error) && (1.e-4f < r);
  error(mask) = sqrt(error)/r;
  error(!mask ) = 1.e10f;
  return !mask;
}

float_m KFParticleBaseSIMD::GetMass( float_v &m, float_v &error ) const 
{
  //* Calculate particle mass
  
  // s = sigma^2 of m2/2

  const float_v BIG = 1.e20f;
  const float_v LocalSmall = 1.e-10f;

  float_v ret = 0.f;

  float_v s = (  fP[3]*fP[3]*fC[9] + fP[4]*fP[4]*fC[14] + fP[5]*fP[5]*fC[20] 
		  + fP[6]*fP[6]*fC[27] 
		+float_v(2.f)*( + fP[3]*fP[4]*fC[13] + fP[5]*(fP[3]*fC[18] + fP[4]*fC[19]) 
		     - fP[6]*( fP[3]*fC[24] + fP[4]*fC[25] + fP[5]*fC[26] )   )
		 ); 
//   float_v m2 = abs(fP[6]*fP[6] - fP[3]*fP[3] - fP[4]*fP[4] - fP[5]*fP[5]);
//   m  = sqrt(m2);
//   if( m>1.e-10 ){
//     if( s>=0 ){
//       error = sqrt(s)/m;
//       return 0;
//     }
//   }
//   error = 1.e20;
  float_v m2 = (fP[6]*fP[6] - fP[3]*fP[3] - fP[4]*fP[4] - fP[5]*fP[5]);

  float_m mask = 0.f <= m2;
  m(mask) = sqrt(m2);
  m(!mask) = -sqrt(-m2);

  mask = (mask && (0.f <= s) && (LocalSmall < m));
  error(mask) = sqrt(s)/m;
  error(!mask) = BIG;

  return !mask;
}


float_m KFParticleBaseSIMD::GetDecayLength( float_v &l, float_v &error ) const 
{
  //* Calculate particle decay length [cm]

  const float_v BIG = 1.e20f;

  float_v x = fP[3];
  float_v y = fP[4];
  float_v z = fP[5];
  float_v t = fP[7];
  float_v x2 = x*x;
  float_v y2 = y*y;
  float_v z2 = z*z;
  float_v p2 = x2+y2+z2;
  l = t*sqrt(p2);

  error = p2*fC[35] + t*t/p2*(x2*fC[9]+y2*fC[14]+z2*fC[20]
        + float_v(2.f)*(x*y*fC[13]+x*z*fC[18]+y*z*fC[19]) )
        + float_v(2.f)*t*(x*fC[31]+y*fC[32]+z*fC[33]);

  float_m mask = ((1.e-4f) < p2);
  error(mask) = sqrt(abs(error));
  error(!mask) = BIG;
  return !mask;
}

float_m KFParticleBaseSIMD::GetDecayLengthXY( float_v &l, float_v &error ) const 
{
  //* Calculate particle decay length in XY projection [cm]

  const float_v BIG = 1.e20f;
  float_v ret = 0.f;

  float_v x = fP[3];
  float_v y = fP[4];
  float_v t = fP[7];
  float_v x2 = x*x;
  float_v y2 = y*y;
  float_v pt2 = x2+y2;
  l = t*sqrt(pt2);

  error = pt2*fC[35] + t*t/pt2*(x2*fC[9]+y2*fC[14] + 2*x*y*fC[13] )
        + float_v(2.f)*t*(x*fC[31]+y*fC[32]);
  float_m mask = ((1.e-4f) < pt2);
  error(mask) = sqrt(abs(error));
  error(!mask) = BIG;
  return !mask;
}


float_m KFParticleBaseSIMD::GetLifeTime( float_v &tauC, float_v &error ) const 
{
  //* Calculate particle decay time [s]

  const float_v BIG = 1.e20f;

  float_v m, dm;
  GetMass( m, dm );
  float_v cTM = (-fP[3]*fC[31] - fP[4]*fC[32] - fP[5]*fC[33] + fP[6]*fC[34]);
  tauC = fP[7]*m;
  error = m*m*fC[35] + 2*fP[7]*cTM + fP[7]*fP[7]*dm*dm;
  float_m mask = (0.f < error);
  error(mask) = sqrt(error);
  error(!mask) = BIG;
  return !mask;
}


void KFParticleBaseSIMD::operator +=( const KFParticleBaseSIMD &Daughter )
{
  //* Add daughter via operator+=

  AddDaughter( Daughter );
}
  
float_v KFParticleBaseSIMD::GetSCorrection( const float_v Part[], const float_v XYZ[] ) const
{
  //* Get big enough correction for S error to let the particle Part be fitted to XYZ point
  
  float_v d[3] = { XYZ[0]-Part[0], XYZ[1]-Part[1], XYZ[2]-Part[2] };
  float_v p2 = Part[3]*Part[3]+Part[4]*Part[4]+Part[5]*Part[5];
//  float_v sigmaS = if3( float_v(float_v(1.e-4)<p2) , ( float_v(10.1)+float_v(3.)*sqrt( d[0]*d[0]+d[1]*d[1]+d[2]*d[2]) )/sqrt(p2) , 1.f);
//  float_v sigmaS = if3( float_v(float_v(1.e-4)<p2) , ( float_v(0.1)+float_v(10.)*sqrt( d[0]*d[0]+d[1]*d[1]+d[2]*d[2]) )/sqrt(p2) , 1.f);

  float_v sigmaS(1.f);
  p2(1.e-4f >= p2) = 1.e-4;
  sigmaS(1.e-4f < p2) = 0.1f+10.f*sqrt( (d[0]*d[0]+d[1]*d[1]+d[2]*d[2])/p2 );
//   float_v sigmaS = GetDStoPoint(XYZ)*0.08;

  return sigmaS;
}

void KFParticleBaseSIMD::GetMeasurement( const float_v XYZ[], float_v m[], float_v V[], Bool_t isAtVtxGuess ) const
{
  //* Get additional covariances V used during measurement

  float_v b[3];
  GetFieldValue( XYZ, b );
  const float_v kCLight =  0.000299792458f;
  b[0]*=kCLight*GetQ(); b[1]*=kCLight*GetQ(); b[2]*=kCLight*GetQ();

  if(!isAtVtxGuess)
    Transport( GetDStoPoint(XYZ), m, V );
  else
  {
    for(int iM=0; iM<8; iM++)
      m[iM] = fP[iM];
    for(int iV=0; iV<8; iV++)
      V[iV] = fC[iV];
  }

  float_v sigmaS = GetSCorrection( m, XYZ );

  float_v h[6];

  h[0] = m[3]*sigmaS;
  h[1] = m[4]*sigmaS;
  h[2] = m[5]*sigmaS;
  h[3] = ( h[1]*b[2]-h[2]*b[1] );
  h[4] = ( h[2]*b[0]-h[0]*b[2] );
  h[5] = ( h[0]*b[1]-h[1]*b[0] );
  
  V[ 0]+= h[0]*h[0];
  V[ 1]+= h[1]*h[0];
  V[ 2]+= h[1]*h[1];
  V[ 3]+= h[2]*h[0];
  V[ 4]+= h[2]*h[1];
  V[ 5]+= h[2]*h[2];

  V[ 6]+= h[3]*h[0];
  V[ 7]+= h[3]*h[1];
  V[ 8]+= h[3]*h[2];
  V[ 9]+= h[3]*h[3];

  V[10]+= h[4]*h[0];
  V[11]+= h[4]*h[1];
  V[12]+= h[4]*h[2];
  V[13]+= h[4]*h[3];
  V[14]+= h[4]*h[4];

  V[15]+= h[5]*h[0];
  V[16]+= h[5]*h[1];
  V[17]+= h[5]*h[2];
  V[18]+= h[5]*h[3];
  V[19]+= h[5]*h[4];
  V[20]+= h[5]*h[5];

}

void KFParticleBaseSIMD::AddDaughter( const KFParticleBaseSIMD &Daughter, Bool_t isAtVtxGuess )
{
  AddDaughterId( Daughter.Id() );

  if( int(fNDF[0])<-1 ){ // first daughter -> just copy
    fNDF   = -1;
    fQ     =  Daughter.GetQ();
    if( float(Daughter.fC[35][0])>0 ){ //TODO Check this: only the first daughter is used here!
      Daughter.GetMeasurement( fVtxGuess, fP, fC, isAtVtxGuess );
    } else {
      for( Int_t i=0; i<8; i++ ) fP[i] = Daughter.fP[i];
      for( Int_t i=0; i<36; i++ ) fC[i] = Daughter.fC[i];
    }
    fSFromDecay = 0;
    fMassHypo = Daughter.fMassHypo;
    SumDaughterMass = Daughter.SumDaughterMass;
    return;
  }

  if(fConstructMethod == 0)
    AddDaughterWithEnergyFit(Daughter,isAtVtxGuess);
  else if(fConstructMethod == 1)
    AddDaughterWithEnergyCalc(Daughter,isAtVtxGuess);
  else if(fConstructMethod == 2)
    AddDaughterWithEnergyFitMC(Daughter,isAtVtxGuess);

  SumDaughterMass += Daughter.SumDaughterMass;
  fMassHypo = -1;
}

void KFParticleBaseSIMD::AddDaughterWithEnergyFit( const KFParticleBaseSIMD &Daughter, Bool_t isAtVtxGuess )
{
  //* Energy considered as an independent veriable, fitted independently from momentum, without any constraints on mass

  //* Add daughter 

//   if(!isAtVtxGuess)
//     TransportToDecayVertex();

  Int_t maxIter = 1;

  if( (!fIsLinearized) && (!isAtVtxGuess) ){
    if( int(fNDF[0])==-1 ){
      float_v ds, ds1;
      GetDStoParticle(Daughter, ds, ds1);      
      TransportToDS( ds );
      float_v m[8];
      float_v mCd[36];       
      Daughter.Transport( ds1, m, mCd );    
      fVtxGuess[0] = .5f*( fP[0] + m[0] );
      fVtxGuess[1] = .5f*( fP[1] + m[1] );
      fVtxGuess[2] = .5f*( fP[2] + m[2] );
    } else {
      fVtxGuess[0] = fP[0];
      fVtxGuess[1] = fP[1];
      fVtxGuess[2] = fP[2]; 
    }
    maxIter = 3;
  }

  for( Int_t iter=0; iter<maxIter; iter++ ){

    float_v *ffP = fP, *ffC = fC;

    float_v m[8], mV[36];

    if( float(Daughter.fC[35][0])>0 ){ //TODO Check this: only the first daughter is used here!
      Daughter.GetMeasurement( fVtxGuess, m, mV, isAtVtxGuess );
    } else {
      for( Int_t i=0; i<8; i++ ) m[i] = Daughter.fP[i];
      for( Int_t i=0; i<36; i++ ) mV[i] = Daughter.fC[i];
    }
    //*

    float_v mS[6]= { ffC[0]+mV[0], 
                  ffC[1]+mV[1], ffC[2]+mV[2], 
                  ffC[3]+mV[3], ffC[4]+mV[4], ffC[5]+mV[5] };
    InvertCholetsky3(mS);

    //* Residual (measured - estimated)

    float_v zeta[3] = { m[0]-ffP[0], m[1]-ffP[1], m[2]-ffP[2] };    


    //* CHt = CH' - D'

    float_v mCHt0[7], mCHt1[7], mCHt2[7];

    mCHt0[0]=ffC[ 0] ;       mCHt1[0]=ffC[ 1] ;       mCHt2[0]=ffC[ 3] ;
    mCHt0[1]=ffC[ 1] ;       mCHt1[1]=ffC[ 2] ;       mCHt2[1]=ffC[ 4] ;
    mCHt0[2]=ffC[ 3] ;       mCHt1[2]=ffC[ 4] ;       mCHt2[2]=ffC[ 5] ;
    mCHt0[3]=ffC[ 6]-mV[ 6]; mCHt1[3]=ffC[ 7]-mV[ 7]; mCHt2[3]=ffC[ 8]-mV[ 8];
    mCHt0[4]=ffC[10]-mV[10]; mCHt1[4]=ffC[11]-mV[11]; mCHt2[4]=ffC[12]-mV[12];
    mCHt0[5]=ffC[15]-mV[15]; mCHt1[5]=ffC[16]-mV[16]; mCHt2[5]=ffC[17]-mV[17];
    mCHt0[6]=ffC[21]-mV[21]; mCHt1[6]=ffC[22]-mV[22]; mCHt2[6]=ffC[23]-mV[23];
  
    //* Kalman gain K = mCH'*S
    
    float_v k0[7], k1[7], k2[7];
    
    for(Int_t i=0;i<7;++i){
      k0[i] = mCHt0[i]*mS[0] + mCHt1[i]*mS[1] + mCHt2[i]*mS[3];
      k1[i] = mCHt0[i]*mS[1] + mCHt1[i]*mS[2] + mCHt2[i]*mS[4];
      k2[i] = mCHt0[i]*mS[3] + mCHt1[i]*mS[4] + mCHt2[i]*mS[5];
    }

   //* New estimation of the vertex position 

    if( iter<maxIter-1 ){
      for(Int_t i=0; i<3; ++i) 
        fVtxGuess[i]= ffP[i] + k0[i]*zeta[0]+k1[i]*zeta[1]+k2[i]*zeta[2];
      continue;
    }

    // last itearation -> update the particle

    //* Add the daughter momentum to the particle momentum
    
    ffP[ 3] += m[ 3];
    ffP[ 4] += m[ 4];
    ffP[ 5] += m[ 5];
    ffP[ 6] += m[ 6];
  
    ffC[ 9] += mV[ 9];
    ffC[13] += mV[13];
    ffC[14] += mV[14];
    ffC[18] += mV[18];
    ffC[19] += mV[19];
    ffC[20] += mV[20];
    ffC[24] += mV[24];
    ffC[25] += mV[25];
    ffC[26] += mV[26];
    ffC[27] += mV[27];
    
 
   //* New estimation of the vertex position r += K*zeta
    
    for(Int_t i=0;i<7;++i) 
      fP[i] = ffP[i] + (k0[i]*zeta[0] + k1[i]*zeta[1] + k2[i]*zeta[2]);
    
    //* New covariance matrix C -= K*(mCH')'

    for(Int_t i=0, k=0;i<7;++i){
      for(Int_t j=0;j<=i;++j,++k){
        fC[k] = ffC[k] - (k0[i]*mCHt0[j] + k1[i]*mCHt1[j] + k2[i]*mCHt2[j] );
      }
    }
  
    //* Calculate Chi^2 
    if( iter == (maxIter-1) ) {
      fNDF  += 2;
      fQ    +=  Daughter.GetQ();
      fSFromDecay = 0.f;    
      fChi2 += (mS[0]*zeta[0] + mS[1]*zeta[1] + mS[3]*zeta[2])*zeta[0]
        +      (mS[1]*zeta[0] + mS[2]*zeta[1] + mS[4]*zeta[2])*zeta[1]
        +      (mS[3]*zeta[0] + mS[4]*zeta[1] + mS[5]*zeta[2])*zeta[2];     
    }

  }
}

void KFParticleBaseSIMD::AddDaughterWithEnergyCalc( const KFParticleBaseSIMD &Daughter, Bool_t isAtVtxGuess )
{
  //* Energy considered as a dependent variable, calculated from the momentum and mass hypothesis

  //* Add daughter 

//   if(!isAtVtxGuess)
//     TransportToDecayVertex();

  Int_t maxIter = 1;

  if( (!fIsLinearized) && (!isAtVtxGuess) ){
    if( int(fNDF[0])==-1 ){
      float_v ds, ds1;
      GetDStoParticle(Daughter, ds, ds1);      
      TransportToDS( ds );
      float_v m[8];
      float_v mCd[36];       
      Daughter.Transport( ds1, m, mCd );    
      fVtxGuess[0] = .5f*( fP[0] + m[0] );
      fVtxGuess[1] = .5f*( fP[1] + m[1] );
      fVtxGuess[2] = .5f*( fP[2] + m[2] );
    } else {
      fVtxGuess[0] = fP[0];
      fVtxGuess[1] = fP[1];
      fVtxGuess[2] = fP[2]; 
    }
    maxIter = 3;
  }

  for( Int_t iter=0; iter<maxIter; iter++ ){

    float_v *ffP = fP, *ffC = fC;

    float_v m[8], mV[36];

    if( float(Daughter.fC[35][0])>0.f ){ //TODO Check this: only the first daughter is used here!
      Daughter.GetMeasurement( fVtxGuess, m, mV, isAtVtxGuess );
    } else {
      for( Int_t i=0; i<8; i++ ) m[i] = Daughter.fP[i];
      for( Int_t i=0; i<36; i++ ) mV[i] = Daughter.fC[i];
    }

    float_v massMf2 = m[6]*m[6] - (m[3]*m[3] + m[4]*m[4] + m[5]*m[5]);
    float_v massRf2 = fP[6]*fP[6] - (fP[3]*fP[3] + fP[4]*fP[4] + fP[5]*fP[5]);

    //*

    float_v mS[6]= { ffC[0]+mV[0], 
                  ffC[1]+mV[1], ffC[2]+mV[2], 
                  ffC[3]+mV[3], ffC[4]+mV[4], ffC[5]+mV[5] };
    InvertCholetsky3(mS);

    //* Residual (measured - estimated)

    float_v zeta[3] = { m[0]-ffP[0], m[1]-ffP[1], m[2]-ffP[2] };    

    //* CHt = CH' - D'

    float_v mCHt0[6], mCHt1[6], mCHt2[6];

    mCHt0[0]=ffC[ 0] ;       mCHt1[0]=ffC[ 1] ;       mCHt2[0]=ffC[ 3] ;
    mCHt0[1]=ffC[ 1] ;       mCHt1[1]=ffC[ 2] ;       mCHt2[1]=ffC[ 4] ;
    mCHt0[2]=ffC[ 3] ;       mCHt1[2]=ffC[ 4] ;       mCHt2[2]=ffC[ 5] ;
    mCHt0[3]=ffC[ 6]-mV[ 6]; mCHt1[3]=ffC[ 7]-mV[ 7]; mCHt2[3]=ffC[ 8]-mV[ 8];
    mCHt0[4]=ffC[10]-mV[10]; mCHt1[4]=ffC[11]-mV[11]; mCHt2[4]=ffC[12]-mV[12];
    mCHt0[5]=ffC[15]-mV[15]; mCHt1[5]=ffC[16]-mV[16]; mCHt2[5]=ffC[17]-mV[17];

    //* Kalman gain K = mCH'*S

    float_v k0[6], k1[6], k2[6];

    for(Int_t i=0;i<6;++i){
      k0[i] = mCHt0[i]*mS[0] + mCHt1[i]*mS[1] + mCHt2[i]*mS[3];
      k1[i] = mCHt0[i]*mS[1] + mCHt1[i]*mS[2] + mCHt2[i]*mS[4];
      k2[i] = mCHt0[i]*mS[3] + mCHt1[i]*mS[4] + mCHt2[i]*mS[5];
    }

   //* New estimation of the vertex position 

    if( iter<maxIter-1 ){
      for(Int_t i=0; i<3; ++i) 
	fVtxGuess[i]= ffP[i] + k0[i]*zeta[0]+k1[i]*zeta[1]+k2[i]*zeta[2];
      continue;
    }

   //* find mf and mVf - optimum value of the measurement and its covariance matrix
    //* mVHt = V*H'
    float_v mVHt0[6], mVHt1[6], mVHt2[6];

    mVHt0[0]= mV[ 0] ; mVHt1[0]= mV[ 1] ; mVHt2[0]= mV[ 3] ;
    mVHt0[1]= mV[ 1] ; mVHt1[1]= mV[ 2] ; mVHt2[1]= mV[ 4] ;
    mVHt0[2]= mV[ 3] ; mVHt1[2]= mV[ 4] ; mVHt2[2]= mV[ 5] ;
    mVHt0[3]= mV[ 6] ; mVHt1[3]= mV[ 7] ; mVHt2[3]= mV[ 8] ;
    mVHt0[4]= mV[10] ; mVHt1[4]= mV[11] ; mVHt2[4]= mV[12] ;
    mVHt0[5]= mV[15] ; mVHt1[5]= mV[16] ; mVHt2[5]= mV[17] ;

    //* Kalman gain Km = mCH'*S

    float_v km0[6], km1[6], km2[6];

    for(Int_t i=0;i<6;++i){
      km0[i] = mVHt0[i]*mS[0] + mVHt1[i]*mS[1] + mVHt2[i]*mS[3];
      km1[i] = mVHt0[i]*mS[1] + mVHt1[i]*mS[2] + mVHt2[i]*mS[4];
      km2[i] = mVHt0[i]*mS[3] + mVHt1[i]*mS[4] + mVHt2[i]*mS[5];
    }

    float_v mf[7] = { m[0], m[1], m[2], m[3], m[4], m[5], m[6] };

    for(Int_t i=0;i<6;++i) 
      mf[i] = mf[i] - km0[i]*zeta[0] - km1[i]*zeta[1] - km2[i]*zeta[2];

    float_v energyMf = sqrt( massMf2 + (mf[3]*mf[3] + mf[4]*mf[4] + mf[5]*mf[5]) );

    float_v mVf[28];
    for(Int_t iC=0; iC<28; iC++)
      mVf[iC] = mV[iC];

    //* hmf = d(energyMf)/d(mf)
    float_v hmf[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    hmf[3](abs(energyMf) >= small) = hmf[3]/energyMf;
    hmf[4](abs(energyMf) >= small) = hmf[4]/energyMf;
    hmf[5](abs(energyMf) >= small) = hmf[5]/energyMf;
    hmf[6] = 0;

    for(Int_t i=0, k=0;i<6;++i){
      for(Int_t j=0;j<=i;++j,++k){
        mVf[k] = mVf[k] - (km0[i]*mVHt0[j] + km1[i]*mVHt1[j] + km2[i]*mVHt2[j] );
      }
    }
    float_v mVf24 = mVf[24], mVf25 = mVf[25], mVf26 = mVf[26];
    mVf[21] = mVf[6 ]*hmf[3] + mVf[10]*hmf[4] + mVf[15]*hmf[5] + mVf[21]*hmf[6];
    mVf[22] = mVf[7 ]*hmf[3] + mVf[11]*hmf[4] + mVf[16]*hmf[5] + mVf[22]*hmf[6];
    mVf[23] = mVf[8 ]*hmf[3] + mVf[12]*hmf[4] + mVf[17]*hmf[5] + mVf[23]*hmf[6];
    mVf[24] = mVf[9 ]*hmf[3] + mVf[13]*hmf[4] + mVf[18]*hmf[5] + mVf[24]*hmf[6];
    mVf[25] = mVf[13]*hmf[3] + mVf[14]*hmf[4] + mVf[19]*hmf[5] + mVf[25]*hmf[6];
    mVf[26] = mVf[18]*hmf[3] + mVf[19]*hmf[4] + mVf[20]*hmf[5] + mVf[26]*hmf[6];
    mVf[27] = mVf[24]*hmf[3] + mVf[25]*hmf[4] + mVf[26]*hmf[5] + (mVf24*hmf[3] + mVf25*hmf[4] + mVf26*hmf[5] + mVf[27]*hmf[6])*hmf[6]; //here mVf[] are already modified

    mf[6] = energyMf;

    //* find rf and mCf - optimum value of the measurement and its covariance matrix

    //* mCCHt = C*H'
    float_v mCCHt0[6], mCCHt1[6], mCCHt2[6];

    mCCHt0[0]=ffC[ 0]; mCCHt1[0]=ffC[ 1]; mCCHt2[0]=ffC[ 3];
    mCCHt0[1]=ffC[ 1]; mCCHt1[1]=ffC[ 2]; mCCHt2[1]=ffC[ 4];
    mCCHt0[2]=ffC[ 3]; mCCHt1[2]=ffC[ 4]; mCCHt2[2]=ffC[ 5];
    mCCHt0[3]=ffC[ 6]; mCCHt1[3]=ffC[ 7]; mCCHt2[3]=ffC[ 8];
    mCCHt0[4]=ffC[10]; mCCHt1[4]=ffC[11]; mCCHt2[4]=ffC[12];
    mCCHt0[5]=ffC[15]; mCCHt1[5]=ffC[16]; mCCHt2[5]=ffC[17];

    //* Kalman gain Krf = mCH'*S

    float_v krf0[6], krf1[6], krf2[6];

    for(Int_t i=0;i<6;++i){
      krf0[i] = mCCHt0[i]*mS[0] + mCCHt1[i]*mS[1] + mCCHt2[i]*mS[3];
      krf1[i] = mCCHt0[i]*mS[1] + mCCHt1[i]*mS[2] + mCCHt2[i]*mS[4];
      krf2[i] = mCCHt0[i]*mS[3] + mCCHt1[i]*mS[4] + mCCHt2[i]*mS[5];
    }
    float_v rf[7] = { ffP[0], ffP[1], ffP[2], ffP[3], ffP[4], ffP[5], ffP[6] };

    for(Int_t i=0;i<6;++i) 
      rf[i] = rf[i] + krf0[i]*zeta[0] + krf1[i]*zeta[1] + krf2[i]*zeta[2];

    float_v energyRf = sqrt( massRf2 + (rf[3]*rf[3] + rf[4]*rf[4] + rf[5]*rf[5]) );

    float_v mCf[28];
    for(Int_t iC=0; iC<28; iC++)
      mCf[iC] = ffC[iC];
    //* hrf = d(Erf)/d(rf)
    float_v hrf[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    hrf[3](abs(energyRf) >= small) = rf[3]/energyRf;
    hrf[4](abs(energyRf) >= small) = rf[4]/energyRf;
    hrf[5](abs(energyRf) >= small) = rf[5]/energyRf;
//    hrf[6] = if3( float_v(abs(E_rf) < small), 0.f, rf[6]/E_rf);
    hrf[6] = 0;

    for(Int_t i=0, k=0;i<6;++i){
      for(Int_t j=0;j<=i;++j,++k){
        mCf[k] = mCf[k] - (krf0[i]*mCCHt0[j] + krf1[i]*mCCHt1[j] + krf2[i]*mCCHt2[j] );
      }
    }
    float_v mCf24 = mCf[24], mCf25 = mCf[25], mCf26 = mCf[26];
    mCf[21] = mCf[6 ]*hrf[3] + mCf[10]*hrf[4] + mCf[15]*hrf[5] + mCf[21]*hrf[6];
    mCf[22] = mCf[7 ]*hrf[3] + mCf[11]*hrf[4] + mCf[16]*hrf[5] + mCf[22]*hrf[6];
    mCf[23] = mCf[8 ]*hrf[3] + mCf[12]*hrf[4] + mCf[17]*hrf[5] + mCf[23]*hrf[6];
    mCf[24] = mCf[9 ]*hrf[3] + mCf[13]*hrf[4] + mCf[18]*hrf[5] + mCf[24]*hrf[6];
    mCf[25] = mCf[13]*hrf[3] + mCf[14]*hrf[4] + mCf[19]*hrf[5] + mCf[25]*hrf[6];
    mCf[26] = mCf[18]*hrf[3] + mCf[19]*hrf[4] + mCf[20]*hrf[5] + mCf[26]*hrf[6];
    mCf[27] = mCf[24]*hrf[3] + mCf[25]*hrf[4] + mCf[26]*hrf[5] + (mCf24*hrf[3] + mCf25*hrf[4] + mCf26*hrf[5] + mCf[27]*hrf[6])*hrf[6]; //here mCf[] are already modified

    for(Int_t iC=21; iC<28; iC++)
    {
      ffC[iC] = mCf[iC];
      mV[iC]  = mVf[iC];
    }

    fP[6] = energyRf + energyMf;
    rf[6] = energyRf;

    //float_v Dvv[3][3]; do not need this
    float_v mDvp[3][3];
    float_v mDpv[3][3];
    float_v mDpp[3][3];
    float_v mDe[7];

    for(int i=0; i<3; i++)
    {
      for(int j=0; j<3; j++)
      {
        mDvp[i][j] = km0[i+3]*mCCHt0[j] + km1[i+3]*mCCHt1[j] + km2[i+3]*mCCHt2[j];
        mDpv[i][j] = km0[i]*mCCHt0[j+3] + km1[i]*mCCHt1[j+3] + km2[i]*mCCHt2[j+3];
        mDpp[i][j] = km0[i+3]*mCCHt0[j+3] + km1[i+3]*mCCHt1[j+3] + km2[i+3]*mCCHt2[j+3];
      }
    }

    mDe[0] = hmf[3]*mDvp[0][0] + hmf[4]*mDvp[1][0] + hmf[5]*mDvp[2][0];
    mDe[1] = hmf[3]*mDvp[0][1] + hmf[4]*mDvp[1][1] + hmf[5]*mDvp[2][1];
    mDe[2] = hmf[3]*mDvp[0][2] + hmf[4]*mDvp[1][2] + hmf[5]*mDvp[2][2];
    mDe[3] = hmf[3]*mDpp[0][0] + hmf[4]*mDpp[1][0] + hmf[5]*mDpp[2][0];
    mDe[4] = hmf[3]*mDpp[0][1] + hmf[4]*mDpp[1][1] + hmf[5]*mDpp[2][1];
    mDe[5] = hmf[3]*mDpp[0][2] + hmf[4]*mDpp[1][2] + hmf[5]*mDpp[2][2];
    mDe[6] = 2*(mDe[3]*hrf[3] + mDe[4]*hrf[4] + mDe[5]*hrf[5]);

    // last itearation -> update the particle

    //* Add the daughter momentum to the particle momentum

    ffP[ 3] += m[ 3];
    ffP[ 4] += m[ 4];
    ffP[ 5] += m[ 5];

    ffC[ 9] += mV[ 9];
    ffC[13] += mV[13];
    ffC[14] += mV[14];
    ffC[18] += mV[18];
    ffC[19] += mV[19];
    ffC[20] += mV[20];
    ffC[24] += mV[24];
    ffC[25] += mV[25];
    ffC[26] += mV[26];
    ffC[27] += mV[27];

    ffC[21] += mDe[0];
    ffC[22] += mDe[1];
    ffC[23] += mDe[2];
    ffC[24] += mDe[3];
    ffC[25] += mDe[4];
    ffC[26] += mDe[5];
    ffC[27] += mDe[6];

   //* New estimation of the vertex position r += K*zeta

    for(Int_t i=0;i<6;++i) 
      fP[i] = ffP[i] + k0[i]*zeta[0] + k1[i]*zeta[1] + k2[i]*zeta[2];

    //* New covariance matrix C -= K*(mCH')'

    for(Int_t i=0, k=0;i<6;++i){
      for(Int_t j=0;j<=i;++j,++k){
	fC[k] = ffC[k] - (k0[i]*mCHt0[j] + k1[i]*mCHt1[j] + k2[i]*mCHt2[j] );
      }
    }

    for(int i=21; i<28; i++) fC[i] = ffC[i];

    //* Calculate Chi^2 

    if( iter == (maxIter-1) ) {
      fNDF  += 2;
      fQ    +=  Daughter.GetQ();
      fSFromDecay = 0.f;    
      fChi2 += (mS[0]*zeta[0] + mS[1]*zeta[1] + mS[3]*zeta[2])*zeta[0]
        +      (mS[1]*zeta[0] + mS[2]*zeta[1] + mS[4]*zeta[2])*zeta[1]
        +      (mS[3]*zeta[0] + mS[4]*zeta[1] + mS[5]*zeta[2])*zeta[2];     
    }
  }
}

void KFParticleBaseSIMD::AddDaughterWithEnergyFitMC( const KFParticleBaseSIMD &Daughter, Bool_t isAtVtxGuess )
{
  //* Energy considered as an independent variable, fitted independently from momentum, without any constraints on mass

  //* Add daughter 

//   if(!isAtVtxGuess)
//     TransportToDecayVertex();

  Int_t maxIter = 1;

  if( (!fIsLinearized) && (!isAtVtxGuess) ){
    if( int(fNDF[0])==-1 ){
      float_v ds, ds1;
      GetDStoParticle(Daughter, ds, ds1);      
      TransportToDS( ds );
      float_v m[8];
      float_v mCd[36];       
      Daughter.Transport( ds1, m, mCd );    
      fVtxGuess[0] = .5f*( fP[0] + m[0] );
      fVtxGuess[1] = .5f*( fP[1] + m[1] );
      fVtxGuess[2] = .5f*( fP[2] + m[2] );
    } else {
      fVtxGuess[0] = fP[0];
      fVtxGuess[1] = fP[1];
      fVtxGuess[2] = fP[2]; 
    }
    maxIter = 3;
  }

  for( Int_t iter=0; iter<maxIter; iter++ ){

    float_v *ffP = fP, *ffC = fC;
    float_v m[8], mV[36];

    if( float(Daughter.fC[35][0])>0.f ){ //TODO Check this: only the first daughter is used here!
      Daughter.GetMeasurement( fVtxGuess, m, mV, isAtVtxGuess );
    } else {
      for( Int_t i=0; i<8; i++ ) m[i] = Daughter.fP[i];
      for( Int_t i=0; i<36; i++ ) mV[i] = Daughter.fC[i];
    }
    //*

    float_v mS[6]= { ffC[0]+mV[0], 
                     ffC[1]+mV[1], ffC[2]+mV[2], 
                     ffC[3]+mV[3], ffC[4]+mV[4], ffC[5]+mV[5] };
    InvertCholetsky3(mS);

    //* Residual (measured - estimated)
    
    float_v zeta[3] = { m[0]-ffP[0], m[1]-ffP[1], m[2]-ffP[2] };    

    
    //* CHt = CH'
    
    float_v mCHt0[7], mCHt1[7], mCHt2[7];
    
    mCHt0[0]=ffC[ 0] ; mCHt1[0]=ffC[ 1] ; mCHt2[0]=ffC[ 3] ;
    mCHt0[1]=ffC[ 1] ; mCHt1[1]=ffC[ 2] ; mCHt2[1]=ffC[ 4] ;
    mCHt0[2]=ffC[ 3] ; mCHt1[2]=ffC[ 4] ; mCHt2[2]=ffC[ 5] ;
    mCHt0[3]=ffC[ 6] ; mCHt1[3]=ffC[ 7] ; mCHt2[3]=ffC[ 8] ;
    mCHt0[4]=ffC[10] ; mCHt1[4]=ffC[11] ; mCHt2[4]=ffC[12] ;
    mCHt0[5]=ffC[15] ; mCHt1[5]=ffC[16] ; mCHt2[5]=ffC[17] ;
    mCHt0[6]=ffC[21] ; mCHt1[6]=ffC[22] ; mCHt2[6]=ffC[23] ;
  
    //* Kalman gain K = mCH'*S
    
    float_v k0[7], k1[7], k2[7];
    
    for(Int_t i=0;i<7;++i){
      k0[i] = mCHt0[i]*mS[0] + mCHt1[i]*mS[1] + mCHt2[i]*mS[3];
      k1[i] = mCHt0[i]*mS[1] + mCHt1[i]*mS[2] + mCHt2[i]*mS[4];
      k2[i] = mCHt0[i]*mS[3] + mCHt1[i]*mS[4] + mCHt2[i]*mS[5];
    }

   //* New estimation of the vertex position 

    if( iter<maxIter-1 ){
      for(Int_t i=0; i<3; ++i) 
	fVtxGuess[i]= ffP[i] + k0[i]*zeta[0]+k1[i]*zeta[1]+k2[i]*zeta[2];
      continue;
    }

    // last itearation -> update the particle

    //* VHt = VH'
    
    float_v mVHt0[7], mVHt1[7], mVHt2[7];
    
    mVHt0[0]=mV[ 0] ; mVHt1[0]=mV[ 1] ; mVHt2[0]=mV[ 3] ;
    mVHt0[1]=mV[ 1] ; mVHt1[1]=mV[ 2] ; mVHt2[1]=mV[ 4] ;
    mVHt0[2]=mV[ 3] ; mVHt1[2]=mV[ 4] ; mVHt2[2]=mV[ 5] ;
    mVHt0[3]=mV[ 6] ; mVHt1[3]=mV[ 7] ; mVHt2[3]=mV[ 8] ;
    mVHt0[4]=mV[10] ; mVHt1[4]=mV[11] ; mVHt2[4]=mV[12] ;
    mVHt0[5]=mV[15] ; mVHt1[5]=mV[16] ; mVHt2[5]=mV[17] ;
    mVHt0[6]=mV[21] ; mVHt1[6]=mV[22] ; mVHt2[6]=mV[23] ;
  
    //* Kalman gain Km = mCH'*S
    
    float_v km0[7], km1[7], km2[7];
    
    for(Int_t i=0;i<7;++i){
      km0[i] = mVHt0[i]*mS[0] + mVHt1[i]*mS[1] + mVHt2[i]*mS[3];
      km1[i] = mVHt0[i]*mS[1] + mVHt1[i]*mS[2] + mVHt2[i]*mS[4];
      km2[i] = mVHt0[i]*mS[3] + mVHt1[i]*mS[4] + mVHt2[i]*mS[5];
    }

    for(Int_t i=0;i<7;++i) 
      ffP[i] = ffP[i] + k0[i]*zeta[0] + k1[i]*zeta[1] + k2[i]*zeta[2];

    for(Int_t i=0;i<7;++i) 
      m[i] = m[i] - km0[i]*zeta[0] - km1[i]*zeta[1] - km2[i]*zeta[2];

    for(Int_t i=0, k=0;i<7;++i){
      for(Int_t j=0;j<=i;++j,++k){
	ffC[k] = ffC[k] - (k0[i]*mCHt0[j] + k1[i]*mCHt1[j] + k2[i]*mCHt2[j] );
      }
    }

    for(Int_t i=0, k=0;i<7;++i){
      for(Int_t j=0;j<=i;++j,++k){
	mV[k] = mV[k] - (km0[i]*mVHt0[j] + km1[i]*mVHt1[j] + km2[i]*mVHt2[j] );
      }
    }

    float_v mDf[7][7];

    for(Int_t i=0;i<7;++i){
      for(Int_t j=0;j<7;++j){
	mDf[i][j] = (km0[i]*mCHt0[j] + km1[i]*mCHt1[j] + km2[i]*mCHt2[j] );
      }
    }

    float_v mJ1[7][7], mJ2[7][7];
    for(Int_t iPar1=0; iPar1<7; iPar1++)
    {
      for(Int_t iPar2=0; iPar2<7; iPar2++)
      {
        mJ1[iPar1][iPar2] = 0.f;
        mJ2[iPar1][iPar2] = 0.f;
      }
    }

    float_v mMassParticle  = ffP[6]*ffP[6] - (ffP[3]*ffP[3] + ffP[4]*ffP[4] + ffP[5]*ffP[5]);
    float_v mMassDaughter  = m[6]*m[6] - (m[3]*m[3] + m[4]*m[4] + m[5]*m[5]);
    mMassParticle(mMassParticle > 0.f) = sqrt(mMassParticle);
    mMassParticle(mMassParticle <= 0.f) = 0.f;
    mMassDaughter(mMassDaughter > 0.f) = sqrt(mMassDaughter);
    mMassDaughter(mMassDaughter <= 0.f) = 0.f;

    float_m mask1 = fMassHypo > -0.5f;
    float_m mask2 = (!mask1) && ( (mMassParticle < SumDaughterMass) || (ffP[6]<0.f)) ;
    SetMassConstraint(ffP,ffC,mJ1,fMassHypo, mask1);
    SetMassConstraint(ffP,ffC,mJ1,SumDaughterMass, mask2);

    float_m mask3 = Daughter.fMassHypo > -0.5f;
    float_m mask4 = ( (!mask3) && ( (mMassDaughter<Daughter.SumDaughterMass) || (m[6]<0.f)) );
    SetMassConstraint(m,mV,mJ2,Daughter.fMassHypo, mask3);
    SetMassConstraint(m,mV,mJ2,Daughter.SumDaughterMass, mask4);

    float_v mDJ[7][7];

    for(Int_t i=0; i<7; i++) {
      for(Int_t j=0; j<7; j++) {
        mDJ[i][j] = 0;
        for(Int_t k=0; k<7; k++) {
          mDJ[i][j] += mDf[i][k]*mJ1[j][k];
        }
      }
    }

    for(Int_t i=0; i<7; ++i){
      for(Int_t j=0; j<7; ++j){
        mDf[i][j]=0;
        for(Int_t l=0; l<7; l++){
          mDf[i][j] += mJ2[i][l]*mDJ[l][j];
        }
      }
    }

    //* Add the daughter momentum to the particle momentum

    ffP[ 3] += m[ 3];
    ffP[ 4] += m[ 4];
    ffP[ 5] += m[ 5];
    ffP[ 6] += m[ 6];

    ffC[ 9] += mV[ 9];
    ffC[13] += mV[13];
    ffC[14] += mV[14];
    ffC[18] += mV[18];
    ffC[19] += mV[19];
    ffC[20] += mV[20];
    ffC[24] += mV[24];
    ffC[25] += mV[25];
    ffC[26] += mV[26];
    ffC[27] += mV[27];

    ffC[6 ] += mDf[3][0]; ffC[7 ] += mDf[3][1]; ffC[8 ] += mDf[3][2];
    ffC[10] += mDf[4][0]; ffC[11] += mDf[4][1]; ffC[12] += mDf[4][2];
    ffC[15] += mDf[5][0]; ffC[16] += mDf[5][1]; ffC[17] += mDf[5][2];
    ffC[21] += mDf[6][0]; ffC[22] += mDf[6][1]; ffC[23] += mDf[6][2];

    ffC[9 ] += mDf[3][3] + mDf[3][3];
    ffC[13] += mDf[4][3] + mDf[3][4]; ffC[14] += mDf[4][4] + mDf[4][4];
    ffC[18] += mDf[5][3] + mDf[3][5]; ffC[19] += mDf[5][4] + mDf[4][5]; ffC[20] += mDf[5][5] + mDf[5][5];
    ffC[24] += mDf[6][3] + mDf[3][6]; ffC[25] += mDf[6][4] + mDf[4][6]; ffC[26] += mDf[6][5] + mDf[5][6]; ffC[27] += mDf[6][6] + mDf[6][6];

   //* New estimation of the vertex position r += K*zeta

    for(Int_t i=0;i<7;++i) 
      fP[i] = ffP[i];

    //* New covariance matrix C -= K*(mCH')'

    for(Int_t i=0, k=0;i<7;++i){
      for(Int_t j=0;j<=i;++j,++k){
        fC[k] = ffC[k];
      }
    }
    //* Calculate Chi^2 

    if( iter == (maxIter-1) ) {
      fNDF  += 2;
      fQ    +=  Daughter.GetQ();
      fSFromDecay = 0.f;    
      fChi2 += (mS[0]*zeta[0] + mS[1]*zeta[1] + mS[3]*zeta[2])*zeta[0]
        +      (mS[1]*zeta[0] + mS[2]*zeta[1] + mS[4]*zeta[2])*zeta[1]
        +      (mS[3]*zeta[0] + mS[4]*zeta[1] + mS[5]*zeta[2])*zeta[2];
    }
  }
}

void KFParticleBaseSIMD::SetProductionVertex( const KFParticleBaseSIMD &Vtx )
{
  //* Set production vertex for the particle, when the particle was not used in the vertex fit

  const float_v *m = Vtx.fP, *mV = Vtx.fC;

  Bool_t noS = ( float(fC[35][0])<=0.f ); // no decay length allowed //TODO Check this: only the first daughter is used here!

  if( noS ){ 
    TransportToDecayVertex();
    fP[7] = 0.f;
    fC[28] = fC[29] = fC[30] = fC[31] = fC[32] = fC[33] = fC[34] = fC[35] = 0.f;
  } else {
    TransportToDS( GetDStoPoint( m ) );
    fP[7] = -fSFromDecay;
    fC[28] = fC[29] = fC[30] = fC[31] = fC[32] = fC[33] = fC[34] = 0.f;
    fC[35] = 0.1;

    Convert(1);
  }

  float_v mAi[6] = {fC[0], fC[1], fC[2], fC[3], fC[4], fC[5]};

  InvertCholetsky3(mAi);
//  InvertSym3( fC, mAi );

  float_v mB[5][3];

  mB[0][0] = fC[ 6]*mAi[0] + fC[ 7]*mAi[1] + fC[ 8]*mAi[3];
  mB[0][1] = fC[ 6]*mAi[1] + fC[ 7]*mAi[2] + fC[ 8]*mAi[4];
  mB[0][2] = fC[ 6]*mAi[3] + fC[ 7]*mAi[4] + fC[ 8]*mAi[5];

  mB[1][0] = fC[10]*mAi[0] + fC[11]*mAi[1] + fC[12]*mAi[3];
  mB[1][1] = fC[10]*mAi[1] + fC[11]*mAi[2] + fC[12]*mAi[4];
  mB[1][2] = fC[10]*mAi[3] + fC[11]*mAi[4] + fC[12]*mAi[5];

  mB[2][0] = fC[15]*mAi[0] + fC[16]*mAi[1] + fC[17]*mAi[3];
  mB[2][1] = fC[15]*mAi[1] + fC[16]*mAi[2] + fC[17]*mAi[4];
  mB[2][2] = fC[15]*mAi[3] + fC[16]*mAi[4] + fC[17]*mAi[5];

  mB[3][0] = fC[21]*mAi[0] + fC[22]*mAi[1] + fC[23]*mAi[3];
  mB[3][1] = fC[21]*mAi[1] + fC[22]*mAi[2] + fC[23]*mAi[4];
  mB[3][2] = fC[21]*mAi[3] + fC[22]*mAi[4] + fC[23]*mAi[5];

  mB[4][0] = fC[28]*mAi[0] + fC[29]*mAi[1] + fC[30]*mAi[3];
  mB[4][1] = fC[28]*mAi[1] + fC[29]*mAi[2] + fC[30]*mAi[4];
  mB[4][2] = fC[28]*mAi[3] + fC[29]*mAi[4] + fC[30]*mAi[5];

  float_v z[3] = { m[0]-fP[0], m[1]-fP[1], m[2]-fP[2] };

  {
    float_v mAVi[6] = { fC[0]-mV[0], fC[1]-mV[1], fC[2]-mV[2], 
			fC[3]-mV[3], fC[4]-mV[4], fC[5]-mV[5] };
    
//    float_v init = InvertSym3( mAVi, mAVi ) ;
    InvertCholetsky3(mAVi);

    float_v dChi2 = ( +(mAVi[0]*z[0] + mAVi[1]*z[1] + mAVi[3]*z[2])*z[0]
                   +(mAVi[1]*z[0] + mAVi[2]*z[1] + mAVi[4]*z[2])*z[1]
                   +(mAVi[3]*z[0] + mAVi[4]*z[1] + mAVi[5]*z[2])*z[2] );
      
      // Take Abs(dChi2) here. Negative value of 'det' or 'dChi2' shows that the particle 
      // was not used in the production vertex fit
      
//    fChi2 += (!init) & abs( dChi2 );
    fChi2 += abs( dChi2 );
    fNDF  += 2;
  }
  
  fP[0] = m[0];
  fP[1] = m[1];
  fP[2] = m[2];
  fP[3]+= mB[0][0]*z[0] + mB[0][1]*z[1] + mB[0][2]*z[2];
  fP[4]+= mB[1][0]*z[0] + mB[1][1]*z[1] + mB[1][2]*z[2];
  fP[5]+= mB[2][0]*z[0] + mB[2][1]*z[1] + mB[2][2]*z[2];
  fP[6]+= mB[3][0]*z[0] + mB[3][1]*z[1] + mB[3][2]*z[2];
  fP[7]+= mB[4][0]*z[0] + mB[4][1]*z[1] + mB[4][2]*z[2];
  
  float_v d0, d1, d2;

  fC[0] = mV[0];
  fC[1] = mV[1];
  fC[2] = mV[2];
  fC[3] = mV[3];
  fC[4] = mV[4];
  fC[5] = mV[5];

  d0= mB[0][0]*mV[0] + mB[0][1]*mV[1] + mB[0][2]*mV[3] - fC[ 6];
  d1= mB[0][0]*mV[1] + mB[0][1]*mV[2] + mB[0][2]*mV[4] - fC[ 7];
  d2= mB[0][0]*mV[3] + mB[0][1]*mV[4] + mB[0][2]*mV[5] - fC[ 8];

  fC[ 6]+= d0;
  fC[ 7]+= d1;
  fC[ 8]+= d2;
  fC[ 9]+= d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];

  d0= mB[1][0]*mV[0] + mB[1][1]*mV[1] + mB[1][2]*mV[3] - fC[10];
  d1= mB[1][0]*mV[1] + mB[1][1]*mV[2] + mB[1][2]*mV[4] - fC[11];
  d2= mB[1][0]*mV[3] + mB[1][1]*mV[4] + mB[1][2]*mV[5] - fC[12];

  fC[10]+= d0;
  fC[11]+= d1;
  fC[12]+= d2;
  fC[13]+= d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];
  fC[14]+= d0*mB[1][0] + d1*mB[1][1] + d2*mB[1][2];

  d0= mB[2][0]*mV[0] + mB[2][1]*mV[1] + mB[2][2]*mV[3] - fC[15];
  d1= mB[2][0]*mV[1] + mB[2][1]*mV[2] + mB[2][2]*mV[4] - fC[16];
  d2= mB[2][0]*mV[3] + mB[2][1]*mV[4] + mB[2][2]*mV[5] - fC[17];

  fC[15]+= d0;
  fC[16]+= d1;
  fC[17]+= d2;
  fC[18]+= d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];
  fC[19]+= d0*mB[1][0] + d1*mB[1][1] + d2*mB[1][2];
  fC[20]+= d0*mB[2][0] + d1*mB[2][1] + d2*mB[2][2];

  d0= mB[3][0]*mV[0] + mB[3][1]*mV[1] + mB[3][2]*mV[3] - fC[21];
  d1= mB[3][0]*mV[1] + mB[3][1]*mV[2] + mB[3][2]*mV[4] - fC[22];
  d2= mB[3][0]*mV[3] + mB[3][1]*mV[4] + mB[3][2]*mV[5] - fC[23];

  fC[21]+= d0;
  fC[22]+= d1;
  fC[23]+= d2;
  fC[24]+= d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];
  fC[25]+= d0*mB[1][0] + d1*mB[1][1] + d2*mB[1][2];
  fC[26]+= d0*mB[2][0] + d1*mB[2][1] + d2*mB[2][2];
  fC[27]+= d0*mB[3][0] + d1*mB[3][1] + d2*mB[3][2];

  d0= mB[4][0]*mV[0] + mB[4][1]*mV[1] + mB[4][2]*mV[3] - fC[28];
  d1= mB[4][0]*mV[1] + mB[4][1]*mV[2] + mB[4][2]*mV[4] - fC[29];
  d2= mB[4][0]*mV[3] + mB[4][1]*mV[4] + mB[4][2]*mV[5] - fC[30];

  fC[28]+= d0;
  fC[29]+= d1;
  fC[30]+= d2;
  fC[31]+= d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];
  fC[32]+= d0*mB[1][0] + d1*mB[1][1] + d2*mB[1][2];
  fC[33]+= d0*mB[2][0] + d1*mB[2][1] + d2*mB[2][2];
  fC[34]+= d0*mB[3][0] + d1*mB[3][1] + d2*mB[3][2];
  fC[35]+= d0*mB[4][0] + d1*mB[4][1] + d2*mB[4][2];
  
  if( noS ){ 
    fP[7] = 0;
    fC[28] = fC[29] = fC[30] = fC[31] = fC[32] = fC[33] = fC[34] = fC[35] = 0;
  } else {
    TransportToDS( fP[7] );
    Convert(0);
  }

  fSFromDecay = 0.f;
}

void KFParticleBaseSIMD::SetMassConstraint( float_v *mP, float_v *mC, float_v mJ[7][7], float_v mass, float_m mask )
{
  //* Set nonlinear mass constraint (Mass) on the state vector mP with a covariance matrix mC.
  
  const float_v energy2 = mP[6]*mP[6], p2 = mP[3]*mP[3]+mP[4]*mP[4]+mP[5]*mP[5], mass2 = mass*mass;

  const float_v a = energy2 - p2 + 2.f*mass2;
  const float_v b = -2.f*(energy2 + p2);
  const float_v c = energy2 - p2 - mass2;

  float_v lambda(Vc::Zero);
  lambda(abs(b) > float_v(1.e-10f)) = -c/b ;

  float_v d = 4.f*energy2*p2 - mass2*(energy2-p2-2.f*mass2);
  float_m qMask = (d >= 0.f) && (abs(a) > (1.e-10f)) ;
  lambda(qMask) = (energy2 + p2 - sqrt(d))/a;

  lambda(mP[6]<0.f) = -1000000.f;

  Int_t iIter=0;
  for(iIter=0; iIter<100; iIter++)
  {
    float_v lambda2 = lambda*lambda;
    float_v lambda4 = lambda2*lambda2;

//    float_v lambda0 = lambda;

    float_v f  = -mass2 * lambda4 + a*lambda2 + b*lambda + c;
    float_v df = -4.f*mass2 * lambda2*lambda + 2.f*a*lambda + b;
    lambda(abs(df) > float_v(1.e-10f)) -= f/df;
//    if(TMath::Abs(lambda0 - lambda) < 1.e-8) break;
  }

  const float_v lpi = 1.f/(1.f + lambda);
  const float_v lmi = 1.f/(1.f - lambda);
  const float_v lp2i = lpi*lpi;
  const float_v lm2i = lmi*lmi;

  float_v lambda2 = lambda*lambda;

  float_v dfl  = -4.f*mass2 * lambda2*lambda + 2.f*a*lambda + b;
  float_v dfx[7] = {0.f};//,0,0,0};
  dfx[0] = -2.f*(1.f + lambda)*(1.f + lambda)*mP[3];
  dfx[1] = -2.f*(1.f + lambda)*(1.f + lambda)*mP[4];
  dfx[2] = -2.f*(1.f + lambda)*(1.f + lambda)*mP[5];
  dfx[3] = 2.f*(1.f - lambda)*(1.f - lambda)*mP[6];
  float_v dlx[4] = {1.f,1.f,1.f,1.f};

  for(int i=0; i<4; i++)
    dlx[i](abs(dfl) > float_v(1.e-10f)) = -dfx[i] / dfl;

  float_v dxx[4] = {mP[3]*lm2i, mP[4]*lm2i, mP[5]*lm2i, -mP[6]*lp2i};

  for(Int_t i=0; i<7; i++)
    for(Int_t j=0; j<7; j++)
      mJ[i][j]=0;
  mJ[0][0] = 1.;
  mJ[1][1] = 1.;
  mJ[2][2] = 1.;

  for(Int_t i=3; i<7; i++)
    for(Int_t j=3; j<7; j++)
      mJ[i][j] = dlx[j-3]*dxx[i-3];

  for(Int_t i=3; i<6; i++)
    mJ[i][i] += lmi;
  mJ[6][6] += lpi;

  float_v mCJ[7][7];

  for(Int_t i=0; i<7; i++) {
    for(Int_t j=0; j<7; j++) {
      mCJ[i][j] = 0;
      for(Int_t k=0; k<7; k++) {
        mCJ[i][j] += mC[IJ(i,k)]*mJ[j][k];
      }
    }
  }

  for(Int_t i=0; i<7; ++i){
    for(Int_t j=0; j<=i; ++j){
      mC[IJ(i,j)](mask) = 0.f;
      for(Int_t l=0; l<7; l++){
        mC[IJ(i,j)](mask) += mJ[i][l]*mCJ[l][j];
      }
    }
  }

  mP[3](mask) *= lmi;
  mP[4](mask) *= lmi;
  mP[5](mask) *= lmi;
  mP[6](mask) *= lpi;
}

void KFParticleBaseSIMD::SetNonlinearMassConstraint( float_v mass )
{
  //* Set nonlinear mass constraint (mass)

  float_v mJ[7][7];
  SetMassConstraint( fP, fC, mJ, mass, float_m(true) );
  fMassHypo = mass;
  SumDaughterMass = mass;
}

void KFParticleBaseSIMD::SetMassConstraint( float_v Mass, float_v SigmaMass )
{  
  //* Set hard( SigmaMass=0 ) or soft (SigmaMass>0) mass constraint 

  fMassHypo = Mass;
  SumDaughterMass = Mass;

  float_v m2 = Mass*Mass;            // measurement, weighted by Mass 
  float_v s2 = m2*SigmaMass*SigmaMass; // sigma^2

  float_v p2 = fP[3]*fP[3] + fP[4]*fP[4] + fP[5]*fP[5]; 
  float_v e0 = sqrt(m2+p2);

  float_v mH[8];
  mH[0] = mH[1] = mH[2] = 0.f;
  mH[3] = -2.f*fP[3]; 
  mH[4] = -2.f*fP[4]; 
  mH[5] = -2.f*fP[5]; 
  mH[6] =  2.f*fP[6];//e0;
  mH[7] = 0.f; 

  float_v zeta = e0*e0 - e0*fP[6];
  zeta = m2 - (fP[6]*fP[6]-p2);
  
  float_v mCHt[8], s2_est=0.f;
  for( Int_t i=0; i<8; ++i ){
    mCHt[i] = 0.0f;
    for (Int_t j=0;j<8;++j) mCHt[i] += Cij(i,j)*mH[j];
    s2_est += mH[i]*mCHt[i];
  }
  
//TODO Next line should be uncommented
//  if( s2_est<1.e-20 ) return; // calculated mass error is already 0, 
                              // the particle can not be constrained on mass

  float_v w2 = 1.f/( s2 + s2_est );
  fChi2 += zeta*zeta*w2;
  fNDF  += 1;
  for( Int_t i=0, ii=0; i<8; ++i ){
    float_v ki = mCHt[i]*w2;
    fP[i]+= ki*zeta;
    for(Int_t j=0;j<=i;++j) fC[ii++] -= ki*mCHt[j];    
  }
}


void KFParticleBaseSIMD::SetNoDecayLength()
{  
  //* Set no decay length for resonances

  TransportToDecayVertex();

  float_v h[8];
  h[0] = h[1] = h[2] = h[3] = h[4] = h[5] = h[6] = 0.f;
  h[7] = 1.f; 

  float_v zeta = 0.f - fP[7];
  for(Int_t i=0;i<8;++i) zeta -= h[i]*(fP[i]-fP[i]);
  
  float_v s = fC[35];   
//  if( s>1.e-20 ) //TODO Should be uncommented!
  {
    s = 1.f/s;
    fChi2 += zeta*zeta*s;
    fNDF  += 1.f;
    for( Int_t i=0, ii=0; i<7; ++i ){
      float_v ki = fC[28+i]*s;
      fP[i]+= ki*zeta;
      for(Int_t j=0;j<=i;++j) fC[ii++] -= ki*fC[28+j];    
    }
  }
  fP[7] = 0.f;
  fC[28] = fC[29] = fC[30] = fC[31] = fC[32] = fC[33] = fC[34] = fC[35] = 0.f;
}

void KFParticleBaseSIMD::Construct( const KFParticleBaseSIMD* vDaughters[], Int_t nDaughters,
                                    const KFParticleBaseSIMD *Parent,  Float_t Mass, Bool_t IsConstrained ,
                                    Bool_t isAtVtxGuess  )
{ 
  //* Full reconstruction in one go

  Int_t maxIter = 1;
  bool wasLinearized = fIsLinearized;
  if( (!fIsLinearized || IsConstrained) && (!isAtVtxGuess) ){
    //fVtxGuess[0] = fVtxGuess[1] = fVtxGuess[2] = 0;  //!!!!
    float_v ds=0.f, ds1=0.f;
    float_v P[8], C[36];
    vDaughters[0]->GetDStoParticle(*vDaughters[1], ds, ds1);
    vDaughters[0]->Transport(ds,P,C);
    fVtxGuess[0] = P[0];
    fVtxGuess[1] = P[1];
    fVtxGuess[2] = P[2];
/*    fVtxGuess[0] = GetX();
    fVtxGuess[1] = GetY();
    fVtxGuess[2] = GetZ();*/

    if(!fIsVtxErrGuess)
    {
      fVtxErrGuess[0] = 1.f;
      fVtxErrGuess[1] = 1.f;
      fVtxErrGuess[2] = 1.f;
      fVtxErrGuess[0](C[0]>0.f) = 10.f*sqrt(C[0]);
      fVtxErrGuess[1](C[2]>0.f) = 10.f*sqrt(C[2]);
      fVtxErrGuess[2](C[5]>0.f) = 10.f*sqrt(C[5]);
    }

    fIsLinearized = 1;
    maxIter = 3;
  }
  else {
    if(!fIsVtxErrGuess)
    {
      fVtxErrGuess[0] = 1.f;
      fVtxErrGuess[1] = 1.f;
      fVtxErrGuess[2] = 1.f;
    }
  }

  float_v constraintC[6];

  if( IsConstrained ){
    for(Int_t i=0;i<6;++i) constraintC[i]=fC[i];
  }
  else {
    for(Int_t i=0;i<6;++i) constraintC[i]=0.f;
//    constraintC[0] = constraintC[2] = constraintC[5] = 100.;

      constraintC[0] = fVtxErrGuess[0]*fVtxErrGuess[0];
      constraintC[2] = fVtxErrGuess[1]*fVtxErrGuess[1];
      constraintC[5] = fVtxErrGuess[2]*fVtxErrGuess[2];
  }

  for( Int_t iter=0; iter<maxIter; iter++ ){

    CleanDaughtersId();
    SetNDaughters(nDaughters);

    fAtProductionVertex = 0;
    fSFromDecay = 0;
    fP[0] = fVtxGuess[0];
    fP[1] = fVtxGuess[1];
    fP[2] = fVtxGuess[2];
    fP[3] = 0.f;
    fP[4] = 0.f;
    fP[5] = 0.f;
    fP[6] = 0.f;
    fP[7] = 0.f;
    SumDaughterMass = 0.f;

    for(Int_t i=0;i<6; ++i) fC[i]=constraintC[i];
    for(Int_t i=6;i<36;++i) fC[i]=0.f;
    fC[35] = 1.f;
    
    fNDF  = IsConstrained ?0 :-3;
    fChi2 =  0.f;
    fQ = 0.f;

    for( Int_t itr =0; itr<nDaughters; itr++ ){
      AddDaughter( *vDaughters[itr], isAtVtxGuess );    
    }
    if( iter<maxIter-1){
      for( Int_t i=0; i<3; i++ ) fVtxGuess[i] = fP[i];  
    }
  }
  fIsLinearized = wasLinearized;    

  if( Mass>=0 ) SetMassConstraint( Mass );
  if( Parent ) SetProductionVertex( *Parent );
}

void KFParticleBaseSIMD::Convert( bool ToProduction )
{
  //* Tricky function - convert the particle error along its trajectory to 
  //* the value which corresponds to its production/decay vertex
  //* It is done by combination of the error of decay length with the position errors

  float_v fld[3];
  {
    GetFieldValue( fP, fld );
    const float_v kCLight =  fQ*0.000299792458f;
    fld[0]*=kCLight; fld[1]*=kCLight; fld[2]*=kCLight;
  }

  float_v h[6];
  
  h[0] = fP[3];
  h[1] = fP[4];
  h[2] = fP[5];
  if( ToProduction ){ h[0]=-h[0]; h[1]=-h[1]; h[2]=-h[2]; } 
  h[3] = h[1]*fld[2]-h[2]*fld[1];
  h[4] = h[2]*fld[0]-h[0]*fld[2];
  h[5] = h[0]*fld[1]-h[1]*fld[0];
  
  float_v c;

  c = fC[28]+h[0]*fC[35];
  fC[ 0]+= h[0]*(c+fC[28]);
  fC[28] = c;

  fC[ 1]+= h[1]*fC[28] + h[0]*fC[29];
  c = fC[29]+h[1]*fC[35];
  fC[ 2]+= h[1]*(c+fC[29]);
  fC[29] = c;

  fC[ 3]+= h[2]*fC[28] + h[0]*fC[30];
  fC[ 4]+= h[2]*fC[29] + h[1]*fC[30];
  c = fC[30]+h[2]*fC[35];
  fC[ 5]+= h[2]*(c+fC[30]);
  fC[30] = c;

  fC[ 6]+= h[3]*fC[28] + h[0]*fC[31];
  fC[ 7]+= h[3]*fC[29] + h[1]*fC[31];
  fC[ 8]+= h[3]*fC[30] + h[2]*fC[31];
  c = fC[31]+h[3]*fC[35];
  fC[ 9]+= h[3]*(c+fC[31]);
  fC[31] = c;
  
  fC[10]+= h[4]*fC[28] + h[0]*fC[32];
  fC[11]+= h[4]*fC[29] + h[1]*fC[32];
  fC[12]+= h[4]*fC[30] + h[2]*fC[32];
  fC[13]+= h[4]*fC[31] + h[3]*fC[32];
  c = fC[32]+h[4]*fC[35];
  fC[14]+= h[4]*(c+fC[32]);
  fC[32] = c;
  
  fC[15]+= h[5]*fC[28] + h[0]*fC[33];
  fC[16]+= h[5]*fC[29] + h[1]*fC[33];
  fC[17]+= h[5]*fC[30] + h[2]*fC[33];
  fC[18]+= h[5]*fC[31] + h[3]*fC[33];
  fC[19]+= h[5]*fC[32] + h[4]*fC[33];
  c = fC[33]+h[5]*fC[35];
  fC[20]+= h[5]*(c+fC[33]);
  fC[33] = c;

  fC[21]+= h[0]*fC[34];
  fC[22]+= h[1]*fC[34];
  fC[23]+= h[2]*fC[34];
  fC[24]+= h[3]*fC[34];
  fC[25]+= h[4]*fC[34];
  fC[26]+= h[5]*fC[34];
}


void KFParticleBaseSIMD::TransportToDecayVertex()
{
  //* Transport the particle to its decay vertex 

  TransportToDS( -fSFromDecay );
  if( fAtProductionVertex ) Convert(0);
  fAtProductionVertex = 0;
}

void KFParticleBaseSIMD::TransportToProductionVertex()
{
  //* Transport the particle to its production vertex 
  
  TransportToDS( -fSFromDecay-fP[7] );
  if( !fAtProductionVertex ) Convert( 1 );
  fAtProductionVertex = 1;
}


void KFParticleBaseSIMD::TransportToDS( float_v dS )
{ 
  //* Transport the particle on dS parameter (SignedPath/Momentum) 
 
  Transport( dS, fP, fC );
  fSFromDecay+= dS;
}

void KFParticleBaseSIMD::TransportToDSLine( float_v dS )
{ 
  //* Transport the particle on dS parameter (SignedPath/Momentum) 
 
  TransportLine( dS, fP, fC );
  fSFromDecay+= dS;
}

void KFParticleBaseSIMD::GetDistanceToVertexLine( const KFParticleBaseSIMD &Vertex, float_v &l, float_v &dl, float_m *isParticleFromVertex ) const 
{
  //* Get dS to a certain space point without field

  float_v c[6] = {Vertex.fC[0]+fC[0], Vertex.fC[1]+fC[1], Vertex.fC[2]+fC[2],
               Vertex.fC[3]+fC[3], Vertex.fC[4]+fC[4], Vertex.fC[5]+fC[5]};

  float_v dx = (Vertex.fP[0]-fP[0]);
  float_v dy = (Vertex.fP[1]-fP[1]);
  float_v dz = (Vertex.fP[2]-fP[2]);

  l = sqrt( dx*dx + dy*dy + dz*dz );
  dl = c[0]*dx*dx + c[2]*dy*dy + c[5]*dz*dz + 2*(c[1]*dx*dy + c[3]*dx*dz + c[4]*dy*dz);

  l(abs(l) < 1.e-8f) = 1.e-8f;
  float_m ok = float_v(Vc::Zero)<=dl;
  dl(!ok) = 1.e8f;
  dl(ok) = sqrt( dl )/l;

  if(isParticleFromVertex)
  {
    *isParticleFromVertex = ok && ( l<float_v(3.f*dl) );
    float_v cosV = dx*fP[3] + dy*fP[4] + dz*fP[5];
//     float_v dCos = dy*dy*fC[14] + dz*dz*fC[20] + dx*dx*fC[9] + 2*dz*fC[15]*fP[3] + c[0]* fP[3]*fP[3] + 
//             2*dz*fC[16]* fP[4] + 2 *c[1] *fP[3] *fP[4] + c[2] *fP[4]*fP[4] + 2 *dz *fC[17]* fP[5] + 
//             2*c[3] *fP[3]* fP[5] + 2 *c[4] *fP[4] *fP[5] + c[5]*fP[5] *fP[5] + 
//             2*dy *(dz *fC[19] + fC[10] *fP[3] + fC[11]* fP[4] + fC[12]* fP[5]) + 
//             2*dx *(dy *fC[13] + dz *fC[18] + fC[6]* fP[3] + fC[7]* fP[4] + fC[8]* fP[5]);
//     ok = float_v(float_v(0)<dCos);
//     dCos = float_v(ok & ( dCos ));
//     dCos = sqrt(dCos);
    *isParticleFromVertex = (*isParticleFromVertex) || (!(*isParticleFromVertex) && (cosV<0.f) ) ;
  }
}


float_v KFParticleBaseSIMD::GetDStoPointBz( float_v B, const float_v xyz[], const float_v* param) 
  const
{ 
  
  if(!param)
    param = fP;
  //* Get dS to a certain space point for Bz field
  const float_v kCLight = 0.000299792458f;
  float_v bq = B*fQ*kCLight;
  float_v pt2 = param[3]*param[3] + param[4]*param[4];
  float_v p2 = pt2 + param[5]*param[5];  
  
  float_v dx = xyz[0] - param[0];
  float_v dy = xyz[1] - param[1]; 
  float_v dz = xyz[2] - param[2]; 
  float_v a = dx*param[3]+dy*param[4];
  float_v dS(Vc::Zero);
  
  float_v abq = bq*a;

  const float_v LocalSmall = 1.e-8f;
  float_m mask = ( abs(bq)<LocalSmall );
  dS(mask && float_m(p2>1.e-4f)) = (a + dz*param[5])/p2;
  if(mask.isFull())
    return dS;
  
  
  dS(!mask) = KFPMath::ATan2( abq, pt2 + bq*(dy*param[3] -dx*param[4]) )/bq;

  float_v bs= bq*dS;

  float_v s = KFPMath::Sin(bs), c = KFPMath::Cos(bs);

  bq(abs(bq) < LocalSmall) = LocalSmall;
  float_v aCoeff = a;
  float_v bCoeff = dx*param[4] - dy*param[3] - pt2/bq;
  
  float_v sz(Vc::Zero);
  sz(abs(param[5]) > 1.e-4f) = dz/param[5];
  float_v kz(Vc::Zero);
  float_v cCoeff = ( sz * (bq*(bCoeff*c - aCoeff*s) - param[5]*param[5]) );
  kz(abs(cCoeff) > 1.e-8f) = (dS*param[5] - dz)*param[5] / cCoeff;
  dS(!mask) += sz*kz;
  
  bs= bq*dS;
  s = KFPMath::Sin(bs), c = KFPMath::Cos(bs);
  
  float_v sB, cB;
  const float_v kOvSqr6 = 1.f/sqrt(float_v(6.f));

  sB(LocalSmall < abs(bs)) = s/bq;
  sB(LocalSmall >= abs(bs)) = (1.f-bs*kOvSqr6)*(1.f+bs*kOvSqr6)*dS;
  cB(LocalSmall < abs(bs)) = (1.f-c)/bq;
  cB(LocalSmall >= abs(bs)) = .5f*sB*bs;

  const float_v px = param[3];
  const float_v py = param[4];

  float_v p[5];
  p[0] = param[0] + sB*px + cB*py;
  p[1] = param[1] - cB*px + sB*py;
  p[2] = param[2] +  dS*param[5];
  p[3] =          c*px + s*py;
  p[4] =         -s*px + c*py;

  dx = xyz[0] - p[0];
  dy = xyz[1] - p[1];
  dz = xyz[2] - p[2];
  a = dx*p[3]+dy*p[4] + dz*param[5];

  abq = bq*a;

  dS(!mask) += KFPMath::ATan2( abq, p2 + bq*(dy*p[3] -dx*p[4]) )/bq;
  
  return dS;
}

float_v KFParticleBaseSIMD::GetDStoPointBy( float_v By, const float_v xyz[] ) 
  const
{ 
  
  //* Get dS to a certain space point for Bz field

  const float_v param[6] = { fP[0], -fP[2], fP[1], fP[3], -fP[5], fP[4] };
  const float_v point[3] = { xyz[0], -xyz[2], xyz[1] };
  
  return GetDStoPointBz(By, point, param);
}

void KFParticleBaseSIMD::GetMaxDistanceToParticleBz(const float_v& B, const KFParticleBaseSIMD &p, float_v &r ) const
{
  //* Get maxium distance between to particles in XY plane
  const float_v kCLight = 0.000299792458f;
  
  const float_v& bq1 = B*fQ*kCLight;
  const float_v& bq2 = B*p.fQ*kCLight;
  const float_m& isStraight1 = abs(bq1) < 1.e-8f;
  const float_m& isStraight2 = abs(bq2) < 1.e-8f;
  
  const float_v& px1 = fP[3];
  const float_v& py1 = fP[4];
//   const float_v& pz1 = fP[5];

  const float_v& px2 = p.fP[3];
  const float_v& py2 = p.fP[4];
//   const float_v& pz2 = p.fP[5];

  const float_v& x01 = fP[0];
  const float_v& y01 = fP[1];
//   const float_v& z01 = fP[2];

  const float_v& x02 = p.fP[0];
  const float_v& y02 = p.fP[1];
//   const float_v& z02 = p.fP[2];

  const float_v& dx0 = (x01 - x02) + (py1/bq1 - py2/bq2);
  const float_v& dy0 = (y01 - y02) - (px1/bq1 - px2/bq2);
  const float_v& pt12 = px1*px1 + py1*py1;
  const float_v& pt22 = px2*px2 + py2*py2;
//   const float_v& dr02 = dx0*dx0 + dy0*dy0;

  float_v yy1 = dy0*pt12;
  float_v xx1 = dx0*pt12;
  float_v yy2 = dy0*pt22;
  float_v xx2 = dx0*pt22;

  float_v dS1(Vc::Zero), dS2(Vc::Zero);

  dS1(!isStraight1 && !isStraight2) = KFPMath::ATan2( (yy1*py1+xx1*px1), (yy1*px1-xx1*py1) )/bq1;
  dS2(!isStraight1 && !isStraight2) = KFPMath::ATan2( (yy2*py2+xx2*px2), (yy2*px2-xx2*py2) )/bq2;
}

void KFParticleBaseSIMD::GetDStoParticleBz( float_v B, const KFParticleBaseSIMD &p, float_v &DS, float_v &DS1, const float_v* param1, const float_v* param2 ) const
{ 
  if(!param1)
  {
    param1 = fP;
    param2 = p.fP;
  }

  //* Get dS to another particle for Bz field
  const float_v kOvSqr6 = 1.f/sqrt(float_v(6.f));
  const float_v kCLight = 0.000299792458f;

  //in XY plane
  //first root    
  const float_v& bq1 = B*fQ*kCLight;
  const float_v& bq2 = B*p.fQ*kCLight;
  const float_m& isStraight1 = abs(bq1) < 1.e-8f;
  const float_m& isStraight2 = abs(bq2) < 1.e-8f;
  
  if( (!(isStraight1.isEmpty())) && (!(isStraight2.isEmpty())))
  {
    GetDStoParticleLine(p, DS, DS1);
    return;
  }
  
  const float_v& px1 = param1[3];
  const float_v& py1 = param1[4];
  const float_v& pz1 = param1[5];

  const float_v& px2 = param2[3];
  const float_v& py2 = param2[4];
  const float_v& pz2 = param2[5];

  const float_v& pt12 = px1*px1 + py1*py1;
  const float_v& pt22 = px2*px2 + py2*py2;

  const float_v& x01 = param1[0];
  const float_v& y01 = param1[1];
  const float_v& z01 = param1[2];

  const float_v& x02 = param2[0];
  const float_v& y02 = param2[1];
  const float_v& z02 = param2[2];

  float_v dS1[2] = {0.f}, dS2[2]={0.f};
  
  const float_v& dx0 = (x01 - x02);
  const float_v& dy0 = (y01 - y02);
  const float_v& dr02 = dx0*dx0 + dy0*dy0;
  const float_v& drp1  = dx0*px1 + dy0*py1;
  const float_v& dxyp1 = dx0*py1 - dy0*px1;
  const float_v& drp2  = dx0*px2 + dy0*py2;
  const float_v& dxyp2 = dx0*py2 - dy0*px2;
  const float_v& p1p2 = px1*px2 + py1*py2;
  const float_v& dp1p2 = px1*py2 - px2*py1;
  
  const float_v& k11 = (bq2*drp1 - dp1p2);
  const float_v& k21 = (bq1*(bq2*dxyp1 - p1p2) + bq2*pt12);
  const float_v& k12 = ((bq1*drp2 - dp1p2));
  const float_v& k22 = (bq2*(bq1*dxyp2 + p1p2) - bq1*pt22);
  
  const float_v& kp = (dxyp1*bq2 - dxyp2*bq1 - p1p2);
  const float_v& kd = dr02/2.f*bq1*bq2 + kp;
  const float_v& c1 = -(bq1*kd + pt12*bq2);
  const float_v& c2 = bq2*kd + pt22*bq1; 
  
  float_v d1 = pt12*pt22 - kd*kd;
  d1(d1 < float_v(Vc::Zero)) = float_v(Vc::Zero);
  d1 = sqrt( d1 );
  float_v d2 = pt12*pt22 - kd*kd;
  d2(d2 < float_v(Vc::Zero)) = float_v(Vc::Zero);
  d2 = sqrt( d2 );
  
  // find two points of closest approach in XY plane
  dS1[0](!isStraight1) = KFPMath::ATan2( (bq1*k11*c1 + k21*d1*bq1), (bq1*k11*d1*bq1 - k21*c1) )/bq1;
  dS2[0](!isStraight2) = KFPMath::ATan2( (bq2*k12*c2 + k22*d2*bq2), (bq2*k12*d2*bq2 - k22*c2) )/bq2;
  dS1[0](isStraight1 && (pt12>float_v(Vc::Zero)) ) = (k11*c1 + k21*d1)/(- k21*c1);
  dS2[0](isStraight2 && (pt22>float_v(Vc::Zero)) ) = (k12*c2 + k22*d2)/(- k22*c2);
  
  dS1[1](!isStraight1) = KFPMath::ATan2( (bq1*k11*c1 - k21*d1*bq1), (-bq1*k11*d1*bq1 - k21*c1) )/bq1;
  dS2[1](!isStraight2) = KFPMath::ATan2( (bq2*k12*c2 - k22*d2*bq2), (-bq2*k12*d2*bq2 - k22*c2) )/bq2;
  dS1[1](isStraight1 && (pt12>float_v(Vc::Zero)) ) = (k11*c1 - k21*d1)/(- k21*c1);
  dS2[1](isStraight2 && (pt22>float_v(Vc::Zero)) )= (k12*c2 - k22*d2)/(- k22*c2);  
  
  //select a point which is close to the primary vertex (with the smallest r)
  
  float_v dr2[2];
  for(int iP = 0; iP<2; iP++)
  {
    const float_v& bs1 = bq1*dS1[iP];
    const float_v& bs2 = bq2*dS2[iP];
    float_v sss = KFPMath::Sin(bs1), ccc = KFPMath::Cos(bs1);
    
    const float_m& bs1Big = abs(bs1) > 1.e-8f;
    const float_m& bs2Big = abs(bs2) > 1.e-8f;
    
    float_v sB(Vc::Zero), cB(Vc::Zero);
    sB(bs1Big) = sss/bq1;
    sB(!bs1Big) = ((1.f-bs1*kOvSqr6)*(1.f+bs1*kOvSqr6)*dS1[iP]);
    cB(bs1Big) = (1.f-ccc)/bq1;
    cB(!bs1Big) = .5f*sB*bs1;
  
    const float_v& x1 = param1[0] + sB*px1 + cB*py1;
    const float_v& y1 = param1[1] - cB*px1 + sB*py1;
    const float_v& z1 = param1[2] + dS1[iP]*param1[5];

    sss = KFPMath::Sin(bs2), ccc = KFPMath::Cos(bs2);

    sB(bs2Big) = sss/bq2;
    sB(!bs2Big) = ((1.f-bs2*kOvSqr6)*(1.f+bs2*kOvSqr6)*dS2[iP]);
    cB(bs2Big) = (1.f-ccc)/bq2;
    cB(!bs2Big) = .5f*sB*bs2;

    const float_v& x2 = param2[0] + sB*px2 + cB*py2;
    const float_v& y2 = param2[1] - cB*px2 + sB*py2;
    const float_v& z2 = param2[2] + dS2[iP]*param2[5];

    float_v dx = (x1-x2);
    float_v dy = (y1-y2);
    float_v dz = (z1-z2);
    
    dr2[iP] = dx*dx + dy*dy + dz*dz;
  }
  
  const float_m isFirstRoot = dr2[0] < dr2[1];
  DS(isFirstRoot)  = dS1[0];
  DS1(isFirstRoot) = dS2[0];
  DS(!isFirstRoot)  = dS1[1];
  DS1(!isFirstRoot) = dS2[1];    
  
  //find correct parts of helices
  int_v n1(Vc::Zero);
  int_v n2(Vc::Zero);
  float_v dzMin = abs( (z01-z02) + DS*pz1 - DS1*pz2 );
  const float_v pi2(6.283185307f);
  
  //TODO optimise for loops for neutral particles
  const float_v& i1Float = -bq1/pi2*(z01/pz1+DS);
  for(int di1=-1; di1<=1; di1++)
  {
    int_v i1(Vc::Zero);
    i1(int_m(!isStraight1)) = int_v(i1Float) + di1;
    
    const float_v& i2Float = ( ((z01-z02) + (DS+pi2*i1/bq1)*pz1)/pz2 - DS1) * bq2/pi2;
    for(int di2 = -1; di2<=1; di2++)
    {
      int_v i2(Vc::Zero);
      i2(int_m(!isStraight2)) = int_v(i2Float) + di2;
      
      const float_v& z1 = z01 + (DS+pi2*i1/bq1)*pz1;
      const float_v& z2 = z02 + (DS1+pi2*i2/bq2)*pz2;
      const float_v& dz = abs( z1-z2 );
    
      n1(int_m(dz < dzMin)) = i1;
      n2(int_m(dz < dzMin)) = i2;
      dzMin(dz < dzMin) = dz;
//     std::cout << "!!!!! " << dz << std::endl;
    }
  }

//   std::cout << "n1 " << n1 << " n2 " << n2 << std::endl;
  DS( !isStraight1) += float_v(n1)*pi2/bq1;
  DS1(!isStraight2) += float_v(n2)*pi2/bq2;

  //add a correction on z-coordinate
//   if(1)
  {
    const float_v& bs1 = bq1*DS;
    const float_v& bs2 = bq2*DS1;
    
    float_v sss = KFPMath::Sin(bs1), ccc = KFPMath::Cos(bs1);
    const float_v& xr1 = sss*px1 - ccc*py1;
    const float_v& yr1 = ccc*px1 + sss*py1;

    sss = KFPMath::Sin(bs2), ccc = KFPMath::Cos(bs2);
    const float_v& xr2 = sss*px2 - ccc*py2;
    const float_v& yr2 = ccc*px2 + sss*py2;
    
    const float_v& br = xr1*xr2 + yr1*yr2;
    const float_v& dx0mod = dx0*bq1*bq2 + py1*bq2 - py2*bq1;
    const float_v& dy0mod = dy0*bq1*bq2 - px1*bq2 + px2*bq1;
    const float_v& ar1 = dx0mod*xr1 + dy0mod*yr1;
    const float_v& ar2 = dx0mod*xr2 + dy0mod*yr2;
    const float_v& cz = (z01 - z02) + DS*pz1 - DS1*pz2;
    
    const float_v& kz11 =  - ar1 + bq1*br + bq2*pz1*pz1;
    const float_v& kz12 =  -bq2*(br+pz1*pz2);
    const float_v& kz21 =   bq1*(br-pz1*pz2);
    const float_v& kz22 =  ar2 - bq2*br - bq1*pz2*pz2;
    
    const float_v& delta = kz11*kz22 - kz12*kz21;
    float_v sz1(Vc::Zero);
    sz1( abs(delta) > 1.e-16f ) = -cz*(pz1*bq2*kz22 - pz2*bq1*kz12) / delta;
    float_v sz2(Vc::Zero);
    sz2( abs(delta) > 1.e-16f )= -cz*(pz2*bq1*kz11 - pz1*bq2*kz21) / delta;

    float_v eq1 = -ar1*sz1 + br*bq1*sz1 - br*bq2*sz2 + bq2*cz*pz1 + bq2*sz1*pz1*pz1 - bq2*pz1*pz2*sz2;
    float_v eq2 = -ar2*sz2 + br*bq1*sz1 - br*bq2*sz2 + bq1*cz*pz2 + bq1*pz1*pz2*sz1 - bq1*pz2*pz2*sz2;
//     std::cout << "!!! eq1 " << eq1 << std::endl;
//     std::cout << "!!! eq2 " << eq2 << std::endl;
//     std::cout << "sz1 " << sz1 << std::endl;
//     std::cout << "sz2 " << sz2 << std::endl;
//     std::cout << std::endl;
//     std::cout << "DS do    " << DS << std::endl;
    DS  += sz1;
//     std::cout << "DS posle " << DS << std::endl;
//     std::cout << "DS1 do    " << DS1 << std::endl;
    DS1 += sz2;
//     std::cout << "DS1 posle " << DS1 << std::endl;
  }  

//   if(1)
//   {
//     float_v eq1(Vc::Zero);
//     float_v eq2(Vc::Zero);
//     
//     float_v bs1 = bq1*DS;
//     float_v bs2 = bq2*DS1;
//     float_v s1 = CAMath::Sin(bs1), c1 = CAMath::Cos(bs1);
//     float_v s2 = CAMath::Sin(bs2), c2 = CAMath::Cos(bs2);
//     
//     const float_m& bs1Big = abs(bs1) > 1.e-8f;
//     float_v sB(Vc::Zero), cB(Vc::Zero);
//     sB(bs1Big) = s1/bq1;
//     sB(!bs1Big) = ((1.f-bs1*kOvSqr6)*(1.f+bs1*kOvSqr6)*DS);
//     cB(bs1Big) = (1.f-c1)/bq1;
//     cB(!bs1Big) = .5f*sB*bs1;
//   
//     float_v x1 = param1[0] + sB*px1 + cB*py1;
//     float_v y1 = param1[1] - cB*px1 + sB*py1;
//     float_v z1 = param1[2] + DS*param1[5];
//     float_v vx1 = s1*param1[4] + c1*param1[3];
//     float_v vy1 = c1*param1[4] - s1*param1[3];
//     float_v vz1 = param1[5];
// 
//     const float_m& bs2Big = abs(bs2) > 1.e-8f;
//     sB(bs2Big) = s2/bq2;
//     sB(!bs2Big) = ((1.f-bs2*kOvSqr6)*(1.f+bs2*kOvSqr6)*DS1);
//     cB(bs2Big) = (1.f-c2)/bq2;
//     cB(!bs2Big) = .5f*sB*bs2;
// 
//     float_v x2 = param2[0] + sB*px2 + cB*py2;
//     float_v y2 = param2[1] - cB*px2 + sB*py2;
//     float_v z2 = param2[2] + DS1*param2[5];
//     float_v vx2 = s2*param2[4] + c2*param2[3];
//     float_v vy2 = c2*param2[4] - s2*param2[3];    
//     float_v vz2 = param2[5];
// 
//     float_v dx = (x1-x2);
//     float_v dy = (y1-y2);
//     float_v dz = (z1-z2);
//     
//     eq1 = dx*vx1 + dy*vy1 + dz*vz1;
//     eq2 = dx*vx2 + dy*vy2 + dz*vz2;
//     const float_v& r1 = sqrt(x1*x1+y1*y1);
//     const float_v& r2 = sqrt(x2*x2+y2*y2);
//     const float_v& dr = sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
//     
//     std::cout << "root " << std::endl;
//     std::cout << "ds to particle eq: " << eq1 << " " << eq2 << std::endl;
//     std::cout << "bq1 " << bq1 << " bq2 " << bq2 << std::endl;
//     std::cout << "ds1 " << DS << " ds2 " << DS1 << std::endl;
//     std::cout << "r1 " << r1 << "   r2 " << r2 << std::endl;
//     std::cout << "dr " << dr << std::endl;
//   }
}

void KFParticleBaseSIMD::GetDStoParticleBy( float_v B, const KFParticleBaseSIMD &p, float_v &DS, float_v &DS1 ) const
{
  const float_v param1[6] = { fP[0], -fP[2], fP[1], fP[3], -fP[5], fP[4] };
  const float_v param2[6] = { p.fP[0], -p.fP[2], p.fP[1], p.fP[3], -p.fP[5], p.fP[4] };
  
  return GetDStoParticleBz(B, p, DS, DS1, param1, param2);
}

float_v KFParticleBaseSIMD::GetDStoPointCBM( const float_v xyz[] ) const
{
  //* Transport the particle on dS, output to P[],C[], for CBM field

//   const float_v *r = fP;
//   float_v p2 = r[3]*r[3] + r[4]*r[4] + r[5]*r[5];
//   float_v dS(Vc::Zero);
//   dS(abs(p2) > float_v(1.E-4f)) = ( r[3]*(xyz[0]-r[0]) + r[4]*(xyz[1]-r[1]) + r[5]*(xyz[2]-r[2]) )/p2;

  float_v dS(Vc::Zero);

//   if( fQ[0]==0 ){
//     dS = GetDStoPointLine( xyz );
//     return dS;
//   }

  float_v fld[3];
  GetFieldValue( fP, fld );
  dS = GetDStoPointBy( fld[1],xyz );

  dS(abs(dS)>1.E3f) = 0.f;

  return dS;
}

void KFParticleBaseSIMD::GetDStoParticleLine( const KFParticleBaseSIMD &p, float_v &dS, float_v &dS1 ) const
{
  const float_v& p12 = fP[3]*fP[3] + fP[4]*fP[4] + fP[5]*fP[5];
  const float_v& p22 = p.fP[3]*p.fP[3] + p.fP[4]*p.fP[4] + p.fP[5]*p.fP[5];
  const float_v& p1p2 = fP[3]*p.fP[3] + fP[4]*p.fP[4] + fP[5]*p.fP[5];

  const float_v& dpx = p.fP[0]-fP[0];
  const float_v& dpy = p.fP[1]-fP[1];
  const float_v& dpz = p.fP[2]-fP[2];
  
  const float_v& drp1 = fP[3]*dpx + fP[4]*dpy + fP[5]*dpz;
  const float_v& drp2 = p.fP[3]*dpx + p.fP[4]*dpy + p.fP[5]*dpz;

  float_v detp =  p1p2*p1p2 - p12*p22;

  detp( abs(detp)<1.e-8f ) = 1.e8;
  
  const float_v& detpi = 1.f/detp;
  
  dS  = (drp2*p1p2 - drp1*p22) * detpi;
  dS1 = (drp2*p12  - drp1*p1p2)* detpi;
}

void KFParticleBaseSIMD::GetDStoParticleCBM( const KFParticleBaseSIMD &p, float_v &dS, float_v &dS1 ) const
{
  //* Transport the particle on dS, output to P[],C[], for CBM field

//     float_v mTy[2];
//     float_v mY[2];
//     float_v mZ[2];
// 
//     mY[0]  = GetY();
//     mZ[0]  = GetZ();
//     mTy[0] = 1.f;
//     mTy[0](abs(GetPz()) > small) = GetPy()/GetPz();
// 
//     mY[1]  = p.GetY();
//     mZ[1]  = p.GetZ();
//     mTy[1] = 1.f;
//     mTy[1]( abs(p.GetPz()) > small) = p.GetPy()/p.GetPz();
// 
//     float_v r0[3] = {0.f};
//     float_v dty = mTy[0]-mTy[1];
//     
//     r0[2](abs(dty) > small) = (mTy[0]*mZ[0]-mTy[1]*mZ[1] + mY[1] -mY[0])/dty;
//     r0[1] = mY[0] + mTy[0]*(r0[2]-mZ[0]);
//     r0[0] = GetX() + GetPx()/GetPz()*(r0[2]-mZ[0]);
//     
//     dS = GetDStoPointCBM(r0);
//     dS1 = p.GetDStoPoint(r0);

//   if( fQ[0]==0 ){
//     GetDStoParticleLine( p, dS, dS1 );
//     return;
//   }

  float_v fld[3];
  GetFieldValue( fP, fld );

  GetDStoParticleBy(fld[1],p,dS,dS1);
}

void KFParticleBaseSIMD::TransportCBM( float_v dS, 
				 float_v P[], float_v C[] ) const
{  
  //* Transport the particle on dS, output to P[],C[], for CBM field
 
  if( float(fQ[0])==0.f ){
    TransportLine( dS, P, C );
    return;
  }

  const float_v kCLight = 0.000299792458f;

  float_v c = fQ*kCLight;

  // construct coefficients 

  float_v 
    px   = fP[3],
    py   = fP[4],
    pz   = fP[5];
      
  float_v sx=0.f, sy=0.f, sz=0.f, syy=0.f, syz=0.f, syyy=0.f, ssx=0.f, ssy=0.f, ssz=0.f, ssyy=0.f, ssyz=0.f, ssyyy=0.f;

  { // get field integrals

    float_v fld[3][3];   
    float_v p0[3], p1[3], p2[3];

    // line track approximation

    p0[0] = fP[0];
    p0[1] = fP[1];
    p0[2] = fP[2];
  
    p2[0] = fP[0] + px*dS;
    p2[1] = fP[1] + py*dS;
    p2[2] = fP[2] + pz*dS;
  
    p1[0] = 0.5f*(p0[0]+p2[0]);
    p1[1] = 0.5f*(p0[1]+p2[1]);
    p1[2] = 0.5f*(p0[2]+p2[2]);

    // first order track approximation
    {
      GetFieldValue( p0, fld[0] );
      GetFieldValue( p1, fld[1] );
      GetFieldValue( p2, fld[2] );

      float_v ssy1 = ( 7.f*fld[0][1] + 6.f*fld[1][1]-fld[2][1] )*c*dS*dS/96.f;
      float_v ssy2 = (   fld[0][1] + 2.f*fld[1][1]         )*c*dS*dS/6.f;

      p1[0] -= ssy1*pz;
      p1[2] += ssy1*px;
      p2[0] -= ssy2*pz;
      p2[2] += ssy2*px;   
    }

    GetFieldValue( p0, fld[0] );
    GetFieldValue( p1, fld[1] );
    GetFieldValue( p2, fld[2] );

    for(int iF1=0; iF1<3; iF1++)
      for(int iF2=0; iF2<3; iF2++)
        fld[iF1][iF2](abs(fld[iF1][iF2]) > float_v(100.f)) = 0.f;

    sx = c*( fld[0][0] + 4*fld[1][0] + fld[2][0] )*dS/6.f;
    sy = c*( fld[0][1] + 4*fld[1][1] + fld[2][1] )*dS/6.f;
    sz = c*( fld[0][2] + 4*fld[1][2] + fld[2][2] )*dS/6.f;

    ssx = c*( fld[0][0] + 2*fld[1][0])*dS*dS/6.f;
    ssy = c*( fld[0][1] + 2*fld[1][1])*dS*dS/6.f;
    ssz = c*( fld[0][2] + 2*fld[1][2])*dS*dS/6.f;

    float_v c2[3][3]    =   { {  5.f, -4.f, -1.f},{  44.f,  80.f,  -4.f},{ 11.f, 44.f, 5.f} }; // /=360.    
    float_v cc2[3][3]    =   { { 38.f,  8.f, -4.f},{ 148.f, 208.f, -20.f},{  3.f, 36.f, 3.f} }; // /=2520.
    for(Int_t n=0; n<3; n++)
      for(Int_t m=0; m<3; m++) 
	{
	  syz += c2[n][m]*fld[n][1]*fld[m][2];
	  ssyz += cc2[n][m]*fld[n][1]*fld[m][2];
	}
 
    syz  *= c*c*dS*dS/360.f;
    ssyz  *= c*c*dS*dS*dS/2520.f;
    
    syy  = c*( fld[0][1] + 4.f*fld[1][1] + fld[2][1] )*dS;
    syyy = syy*syy*syy / 1296.f;
    syy  = syy*syy/72.f;

    ssyy = ( fld[0][1]*( 38.f*fld[0][1] + 156.f*fld[1][1]  -   fld[2][1] )+
	    fld[1][1]*(              208.f*fld[1][1]  +16.f*fld[2][1] )+
	    fld[2][1]*(                             3.f*fld[2][1] )  
	    )*dS*dS*dS*c*c/2520.f;
    ssyyy = 
      (
       fld[0][1]*( fld[0][1]*( 85.f*fld[0][1] + 526.f*fld[1][1]  - 7.f*fld[2][1] )+
		 fld[1][1]*(             1376.f*fld[1][1]  +84.f*fld[2][1] )+
		 fld[2][1]*(                            19.f*fld[2][1] )  )+
       fld[1][1]*( fld[1][1]*(             1376.f*fld[1][1] +256.f*fld[2][1] )+
		 fld[2][1]*(                            62.f*fld[2][1] )  )+
       fld[2][1]*fld[2][1]  *(                             3.f*fld[2][1] )       
       )*dS*dS*dS*dS*c*c*c/90720.f;    
 
  }

  float_v mJ[11];

  mJ[0]=dS-ssyy;  mJ[1]=ssx;  mJ[2]=ssyyy-ssy;
  mJ[3]=-ssz;     mJ[4]=dS;  mJ[5]=ssx+ssyz;

  mJ[6]=1-syy;   mJ[7]=sx;  mJ[8]=syyy-sy;
  mJ[9]=-sz;                mJ[10]=sx+syz;



  P[0] = fP[0] + mJ[0]*px + mJ[1]*py + mJ[2]*pz;
  P[1] = fP[1] + mJ[3]*px + mJ[4]*py + mJ[5]*pz;
  P[2] = fP[2] - mJ[2]*px - mJ[1]*py + mJ[0]*pz;
  P[3] =  mJ[6]*px + mJ[7]*py + mJ[8]*pz;
  P[4] =  mJ[9]*px +       py + mJ[10]*pz;
  P[5] = -mJ[8]*px - mJ[7]*py + mJ[6]*pz;
  P[6] = fP[6];
  P[7] = fP[7];

  if(C!=fC)
  {
    for(int iC=0; iC<36; iC++)
      C[iC] = fC[iC];
  }

  multQSQt1( mJ, C);

//   float_v mJ[8][8];
//   for( Int_t i=0; i<8; i++ ) for( Int_t j=0; j<8; j++) mJ[i][j]=0;
// 
//   mJ[0][0]=1; mJ[0][1]=0; mJ[0][2]=0; mJ[0][3]=dS-ssyy;  mJ[0][4]=ssx;  mJ[0][5]=ssyyy-ssy;
//   mJ[1][0]=0; mJ[1][1]=1; mJ[1][2]=0; mJ[1][3]=-ssz;     mJ[1][4]=dS;  mJ[1][5]=ssx+ssyz;
//   mJ[2][0]=0; mJ[2][1]=0; mJ[2][2]=1; mJ[2][3]=ssy-ssyyy; mJ[2][4]=-ssx; mJ[2][5]=dS-ssyy;
//   
//   mJ[3][0]=0; mJ[3][1]=0; mJ[3][2]=0; mJ[3][3]=1-syy;   mJ[3][4]=sx;  mJ[3][5]=syyy-sy;
//   mJ[4][0]=0; mJ[4][1]=0; mJ[4][2]=0; mJ[4][3]=-sz;     mJ[4][4]=1;   mJ[4][5]=sx+syz;
//   mJ[5][0]=0; mJ[5][1]=0; mJ[5][2]=0; mJ[5][3]=sy-syyy; mJ[5][4]=-sx; mJ[5][5]=1-syy;
//   mJ[6][6] = mJ[7][7] = 1;
//   
//   P[0] = fP[0] + mJ[0][3]*px + mJ[0][4]*py + mJ[0][5]*pz;
//   P[1] = fP[1] + mJ[1][3]*px + mJ[1][4]*py + mJ[1][5]*pz;
//   P[2] = fP[2] + mJ[2][3]*px + mJ[2][4]*py + mJ[2][5]*pz;
//   P[3] =        mJ[3][3]*px + mJ[3][4]*py + mJ[3][5]*pz;
//   P[4] =        mJ[4][3]*px + mJ[4][4]*py + mJ[4][5]*pz;
//   P[5] =        mJ[5][3]*px + mJ[5][4]*py + mJ[5][5]*pz;
//   P[6] = fP[6];
//   P[7] = fP[7];

//  MultQSQt( mJ, fC, C);

}

void KFParticleBaseSIMD::multQSQt1( const float_v J[11], float_v S[] )
{
  const float_v A00 = S[0 ]+S[6 ]*J[0]+S[10]*J[1]+S[15]*J[2];
  const float_v A10 = S[1 ]+S[7 ]*J[0]+S[11]*J[1]+S[16]*J[2];
  const float_v A20 = S[3 ]+S[8 ]*J[0]+S[12]*J[1]+S[17]*J[2];
  const float_v A30 = S[6 ]+S[9 ]*J[0]+S[13]*J[1]+S[18]*J[2];
  const float_v A40 = S[10]+S[13]*J[0]+S[14]*J[1]+S[19]*J[2];
  const float_v A50 = S[15]+S[18]*J[0]+S[19]*J[1]+S[20]*J[2];
  const float_v A60 = S[21]+S[24]*J[0]+S[25]*J[1]+S[26]*J[2];
  const float_v A70 = S[28]+S[31]*J[0]+S[32]*J[1]+S[33]*J[2];

  S[0 ] = A00+J[0]*A30+J[1]*A40+J[2]*A50;
  S[1 ] = A10+J[3]*A30+J[4]*A40+J[5]*A50;
  S[3 ] = A20-J[2]*A30-J[1]*A40+J[0]*A50;
  S[6 ] = J[6]*A30+J[7]*A40+J[8]*A50;
  S[10] = J[9]*A30+        A40+J[10]*A50;
  S[15] = -J[8]*A30-J[7]*A40+J[6]*A50;
  S[21] = A60;
  S[28] = A70;

//  const float_v A01 = S[1 ]+S[6 ]*J[3]+S[10]*J[4]+S[15]*J[5];
  const float_v A11 = S[2 ]+S[7 ]*J[3]+S[11]*J[4]+S[16]*J[5];
  const float_v A21 = S[4 ]+S[8 ]*J[3]+S[12]*J[4]+S[17]*J[5];
  const float_v A31 = S[7 ]+S[9 ]*J[3]+S[13]*J[4]+S[18]*J[5];
  const float_v A41 = S[11]+S[13]*J[3]+S[14]*J[4]+S[19]*J[5];
  const float_v A51 = S[16]+S[18]*J[3]+S[19]*J[4]+S[20]*J[5];
  const float_v A61 = S[22]+S[24]*J[3]+S[25]*J[4]+S[26]*J[5];
  const float_v A71 = S[29]+S[31]*J[3]+S[32]*J[4]+S[33]*J[5];

  S[2 ] = A11+J[3]*A31+J[4]*A41+J[5]*A51;
  S[4 ] = A21-J[2]*A31-J[1]*A41+J[0]*A51;
  S[7 ] = J[6]*A31+J[7]*A41+J[8]*A51;
  S[11] = J[9]*A31+        A41+J[10]*A51;
  S[16] = -J[8]*A31-J[7]*A41+J[6]*A51;
  S[22] = A61;
  S[29] = A71;

//  const float_v A02 = S[3 ]+S[6 ]*J[2][3]+S[10]*J[2][4]+S[15]*J[2][5];
//  const float_v A12 = S[4 ]+S[7 ]*J[2][3]+S[11]*J[2][4]+S[16]*J[2][5];
  const float_v A22 = S[5 ]-S[8 ]*J[2]-S[12]*J[1]+S[17]*J[0];
  const float_v A32 = S[8 ]-S[9 ]*J[2]-S[13]*J[1]+S[18]*J[0];
  const float_v A42 = S[12]-S[13]*J[2]-S[14]*J[1]+S[19]*J[0];
  const float_v A52 = S[17]-S[18]*J[2]-S[19]*J[1]+S[20]*J[0];
  const float_v A62 = S[23]-S[24]*J[2]-S[25]*J[1]+S[26]*J[0];
  const float_v A72 = S[30]-S[31]*J[2]-S[32]*J[1]+S[33]*J[0];

  S[5 ] = A22-J[2]*A32-J[1]*A42+J[0]*A52;
  S[8 ] = J[6]*A32+J[7]*A42+J[8]*A52;
  S[12] = J[9]*A32+        A42+J[10]*A52;
  S[17] = -J[8]*A32-J[7]*A42+J[6]*A52;
  S[23] = A62;
  S[30] = A72;

//  const float_v A03 = S[6] *J[6]+S[10]*J[7]+S[15]*J[8];
//  const float_v A13 = S[7] *J[6]+S[11]*J[7]+S[16]*J[8];
//  const float_v A23 = S[8] *J[6]+S[12]*J[7]+S[17]*J[8];
  const float_v A33 = S[9] *J[6]+S[13]*J[7]+S[18]*J[8];
  const float_v A43 = S[13]*J[6]+S[14]*J[7]+S[19]*J[8];
  const float_v A53 = S[18]*J[6]+S[19]*J[7]+S[20]*J[8];
  const float_v A63 = S[24]*J[6]+S[25]*J[7]+S[26]*J[8];
  const float_v A73 = S[31]*J[6]+S[32]*J[7]+S[33]*J[8];

//  const float_v A04 = S[6] *J[9]+S[10]*J[4][4]+S[15]*J[10];
//  const float_v A14 = S[7] *J[9]+S[11]*J[4][4]+S[16]*J[10];
//  const float_v A24 = S[8] *J[9]+S[12]*J[4][4]+S[17]*J[10];
  const float_v A34 = S[9] *J[9]+S[13]+S[18]*J[10];
  const float_v A44 = S[13]*J[9]+S[14]+S[19]*J[10];
  const float_v A54 = S[18]*J[9]+S[19]+S[20]*J[10];
  const float_v A64 = S[24]*J[9]+S[25]+S[26]*J[10];
  const float_v A74 = S[31]*J[9]+S[32]+S[33]*J[10];

//  const float_v A05 = S[6] *J[5][3]+S[10]*J[5][4]+S[15]*J[6];
//  const float_v A15 = S[7] *J[5][3]+S[11]*J[5][4]+S[16]*J[6];
//  const float_v A25 = S[8] *J[5][3]+S[12]*J[5][4]+S[17]*J[6];
  const float_v A35 = -S[9] *J[8]-S[13]*J[7]+S[18]*J[6];
  const float_v A45 = -S[13]*J[8]-S[14]*J[7]+S[19]*J[6];
  const float_v A55 = -S[18]*J[8]-S[19]*J[7]+S[20]*J[6];
  const float_v A65 = -S[24]*J[8]-S[25]*J[7]+S[26]*J[6];
  const float_v A75 = -S[31]*J[8]-S[32]*J[7]+S[33]*J[6];


  S[9 ] = J[6]*A33+J[7]*A43+J[8]*A53;
  S[13] = J[9]*A33+        A43+J[10]*A53;
  S[18] = -J[8]*A33-J[7]*A43+J[6]*A53;
  S[24] = A63;
  S[31] = A73;

  S[14] = J[9]*A34+        A44+J[10]*A54;
  S[19] = -J[8]*A34-J[7]*A44+J[6]*A54;
  S[25] = A64;
  S[32] = A74;

  S[20] = -J[8]*A35-J[7]*A45+J[6]*A55;
  S[26] = A65;
  S[33] = A75;

//S[27] = S27;
//S[34] = S34;
//S[35] = S35;
}

void KFParticleBaseSIMD::TransportBz( float_v b, float_v t,
				     float_v p[], float_v e[] ) const 
{ 
  //* Transport the particle on dS, output to P[],C[], for Bz field
 
  const float_v kCLight = 0.000299792458f;
  b = b*fQ*kCLight;
  float_v bs= b*t;
  float_v s = KFPMath::Sin(bs), c = KFPMath::Cos(bs);

  float_v sB, cB;

  const float_v kOvSqr6 = 1.f/sqrt(float_v(6.f));
  const float_v LocalSmall = 1.e-10f;

  b(abs(bs) <= LocalSmall) = LocalSmall;
  sB(LocalSmall < abs(bs)) = s/b;
  sB(LocalSmall >= abs(bs)) = (1.f-bs*kOvSqr6)*(1.f+bs*kOvSqr6)*t;
  cB(LocalSmall < abs(bs)) = (1.f-c)/b;
  cB(LocalSmall >= abs(bs)) = .5f*sB*bs;
  
  float_v px = fP[3];
  float_v py = fP[4];
  float_v pz = fP[5];

  p[0] = fP[0] + sB*px + cB*py;
  p[1] = fP[1] - cB*px + sB*py;
  p[2] = fP[2] +  t*pz;
  p[3] =          c*px + s*py;
  p[4] =         -s*px + c*py;
  p[5] = fP[5];
  p[6] = fP[6];
  p[7] = fP[7];

  /* 
  float_v mJ[8][8] = { {1,0,0,   sB, cB,  0, 0, 0 },
			{0,1,0,  -cB, sB,  0, 0, 0 },
			{0,0,1,    0,  0,  t, 0, 0 },
			{0,0,0,    c,  s,  0, 0, 0 },
			{0,0,0,   -s,  c,  0, 0, 0 },
			{0,0,0,    0,  0,  1, 0, 0 },
			{0,0,0,    0,  0,  0, 1, 0 },
			{0,0,0,    0,  0,  0, 0, 1 }  };
  float_v mA[8][8];
  for( Int_t k=0,i=0; i<8; i++)
    for( Int_t j=0; j<=i; j++, k++ ) mA[i][j] = mA[j][i] = fC[k]; 

  float_v mJC[8][8];
  for( Int_t i=0; i<8; i++ )
    for( Int_t j=0; j<8; j++ ){
      mJC[i][j]=0;
      for( Int_t k=0; k<8; k++ ) mJC[i][j]+=mJ[i][k]*mA[k][j];
    }
  
  for( Int_t k=0,i=0; i<8; i++)
    for( Int_t j=0; j<=i; j++, k++ ){
      e[k] = 0;
      for( Int_t l=0; l<8; l++ ) e[k]+=mJC[i][l]*mJ[j][l];
    }
  
  return;
  */

  float_v 
    c6=fC[6], c7=fC[7], c8=fC[8], c17=fC[17], c18=fC[18],
    c24 = fC[24], c31 = fC[31];

  float_v 
    cBC13 = cB*fC[13],
    mJC13 = c7 - cB*fC[9] + sB*fC[13],
    mJC14 = fC[11] - cBC13 + sB*fC[14],
    mJC23 = c8 + t*c18,
    mJC24 = fC[12] + t*fC[19],
    mJC33 = c*fC[9] + s*fC[13],
    mJC34 = c*fC[13] + s*fC[14],
    mJC43 = -s*fC[9] + c*fC[13],
    mJC44 = -s*fC[13] + c*fC[14];


  e[0]= fC[0] + 2.f*(sB*c6 + cB*fC[10]) + (sB*fC[9] + 2*cBC13)*sB + cB*cB*fC[14];
  e[1]= fC[1] - cB*c6 + sB*fC[10] + mJC13*sB + mJC14*cB;
  e[2]= fC[2] - cB*c7 + sB*fC[11] - mJC13*cB + mJC14*sB;
  e[3]= fC[3] + t*fC[15] + mJC23*sB + mJC24*cB;
  e[4]= fC[4] + t*fC[16] - mJC23*cB + mJC24*sB;

  e[15]= fC[15] + c18*sB + fC[19]*cB;
  e[16]= fC[16] - c18*cB + fC[19]*sB;
  e[17]= c17 + fC[20]*t;
  e[18]= c18*c + fC[19]*s;
  e[19]= -c18*s + fC[19]*c;

  e[5]= fC[5] + (c17 + e[17] )*t;

  e[6]= c*c6 + s*fC[10] + mJC33*sB + mJC34*cB;
  e[7]= c*c7 + s*fC[11] - mJC33*cB + mJC34*sB;
  e[8]= c*c8 + s*fC[12] + e[18]*t;
  e[9]= mJC33*c + mJC34*s;
  e[10]= -s*c6 + c*fC[10] + mJC43*sB + mJC44*cB;

    
  e[11]= -s*c7 + c*fC[11] - mJC43*cB + mJC44*sB;
  e[12]= -s*c8 + c*fC[12] + e[19]*t;
  e[13]= mJC43*c + mJC44*s;
  e[14]= -mJC43*s + mJC44*c;
  e[20]= fC[20];
  e[21]= fC[21] + fC[25]*cB + c24*sB;
  e[22]= fC[22] - c24*cB + fC[25]*sB;
  e[23]= fC[23] + fC[26]*t;
  e[24]= c*c24 + s*fC[25];
  e[25]= c*fC[25] - c24*s;
  e[26]= fC[26];
  e[27]= fC[27];
  e[28]= fC[28] + fC[32]*cB + c31*sB;
  e[29]= fC[29] - c31*cB + fC[32]*sB;
  e[30]= fC[30] + fC[33]*t;
  e[31]= c*c31 + s*fC[32];
  e[32]= c*fC[32] - s*c31;
  e[33]= fC[33];
  e[34]= fC[34];
  e[35]= fC[35];     
}


float_v KFParticleBaseSIMD::GetDistanceFromVertex( const KFParticleBaseSIMD &Vtx ) const
{
  //* Calculate distance from vertex [cm]

  return GetDistanceFromVertex( Vtx.fP );
}

float_v KFParticleBaseSIMD::GetDistanceFromVertex( const float_v vtx[] ) const
{
  //* Calculate distance from vertex [cm]

  float_v mP[8], mC[36];  
  Transport( GetDStoPoint(vtx), mP, mC );
  float_v d[3]={ vtx[0]-mP[0], vtx[1]-mP[1], vtx[2]-mP[2]};
  return sqrt( d[0]*d[0]+d[1]*d[1]+d[2]*d[2] );
}

float_v KFParticleBaseSIMD::GetDistanceFromParticle( const KFParticleBaseSIMD &p ) 
  const
{ 
  //* Calculate distance to other particle [cm]

  float_v dS, dS1;
  GetDStoParticle( p, dS, dS1 );   
  float_v mP[8], mC[36], mP1[8], mC1[36];
  Transport( dS, mP, mC ); 
  p.Transport( dS1, mP1, mC1 ); 
  float_v dx = mP[0]-mP1[0]; 
  float_v dy = mP[1]-mP1[1]; 
  float_v dz = mP[2]-mP1[2]; 
  return sqrt(dx*dx+dy*dy+dz*dz);
}

float_v KFParticleBaseSIMD::GetDeviationFromVertex( const KFParticleBaseSIMD &Vtx ) const
{
  //* Calculate sqrt(Chi2/ndf) deviation from vertex

  return GetDeviationFromVertex( Vtx.fP, Vtx.fC );
}


float_v KFParticleBaseSIMD::GetDeviationFromVertex( const float_v v[], const float_v Cv[] ) const
{
  //* Calculate sqrt(Chi2/ndf) deviation from vertex
  //* v = [xyz], Cv=[Cxx,Cxy,Cyy,Cxz,Cyz,Czz]-covariance matrix

  float_v mP[8];
  float_v mC[36];
  
  Transport( GetDStoPoint(v), mP, mC );  

// for(int i=0; i<8; i++)
// mP[i] = fP[i];
// 
// for(int i=0; i<36; i++)
// mC[i] = fC[i];

  float_v d[3]={ v[0]-mP[0], v[1]-mP[1], v[2]-mP[2]};

  float_v sigmaS = .1f+10.f*sqrt( (d[0]*d[0]+d[1]*d[1]+d[2]*d[2])/
                             (mP[3]*mP[3]+mP[4]*mP[4]+mP[5]*mP[5])  );

   
  float_v h[3] = { mP[3]*sigmaS*0, mP[4]*sigmaS*0, mP[5]*sigmaS*0 };       
  
  float_v mSi[6] = 
    { mC[0] +h[0]*h[0], 
      mC[1] +h[1]*h[0], mC[2] +h[1]*h[1], 
      mC[3] +h[2]*h[0], mC[4] +h[2]*h[1], mC[5] +h[2]*h[2] };

  if( Cv ){
    mSi[0]+=Cv[0];
    mSi[1]+=Cv[1];
    mSi[2]+=Cv[2];
    mSi[3]+=Cv[3];
    mSi[4]+=Cv[4];
    mSi[5]+=Cv[5];
  }
  
//   float_v mS[6];
// 
//   mS[0] = mSi[2]*mSi[5] - mSi[4]*mSi[4];
//   mS[1] = mSi[3]*mSi[4] - mSi[1]*mSi[5];
//   mS[2] = mSi[0]*mSi[5] - mSi[3]*mSi[3];
//   mS[3] = mSi[1]*mSi[4] - mSi[2]*mSi[3];
//   mS[4] = mSi[1]*mSi[3] - mSi[0]*mSi[4];
//   mS[5] = mSi[0]*mSi[2] - mSi[1]*mSi[1];	 
//       
//   float_v s = ( mSi[0]*mS[0] + mSi[1]*mS[1] + mSi[3]*mS[3] );
//   s = if3( float_v( abs(s) > 1.E-8 )  , 1.f/s , 0.f);
// 
//   return sqrt( abs(s*( ( mS[0]*d[0] + mS[1]*d[1] + mS[3]*d[2])*d[0]
// 		   +(mS[1]*d[0] + mS[2]*d[1] + mS[4]*d[2])*d[1]
// 		   +(mS[3]*d[0] + mS[4]*d[1] + mS[5]*d[2])*d[2] ))/2);

  InvertCholetsky3(mSi);
  return sqrt( ( ( mSi[0]*d[0] + mSi[1]*d[1] + mSi[3]*d[2])*d[0]
                     +(mSi[1]*d[0] + mSi[2]*d[1] + mSi[4]*d[2])*d[1]
                     +(mSi[3]*d[0] + mSi[4]*d[1] + mSi[5]*d[2])*d[2] ));
}


float_v KFParticleBaseSIMD::GetDeviationFromParticle( const KFParticleBaseSIMD &p ) const
{ 
  //* Calculate sqrt(Chi2/ndf) deviation from other particle

//   float_v dS, dS1;
//   GetDStoParticle( p, dS, dS1 );   
//   float_v mP1[8], mC1[36];
//   p.Transport( dS1, mP1, mC1 ); 
// 
//   float_v d[3]={ fP[0]-mP1[0], fP[1]-mP1[1], fP[2]-mP1[2]};
// 
//   float_v sigmaS = .1f+10.f*sqrt( (d[0]*d[0]+d[1]*d[1]+d[2]*d[2])/
// 					(mP1[3]*mP1[3]+mP1[4]*mP1[4]+mP1[5]*mP1[5])  );
// 
//   float_v h[3] = { mP1[3]*sigmaS, mP1[4]*sigmaS, mP1[5]*sigmaS };       
//   
//   mC1[0] +=h[0]*h[0];
//   mC1[1] +=h[1]*h[0]; 
//   mC1[2] +=h[1]*h[1]; 
//   mC1[3] +=h[2]*h[0]; 
//   mC1[4] +=h[2]*h[1];
//   mC1[5] +=h[2]*h[2];
// 
//   return GetDeviationFromVertex( mP1, mC1 )*float_v(sqrt(2.f));
  
  float_v dS1, dS2;
  GetDStoParticle( p, dS1, dS2 );   
  
  float_v mP1[8], mC1[36];
  Transport( dS1, mP1, mC1 );
  float_v mP2[8], mC2[36];
  p.Transport( dS2, mP2, mC2 );
    
  float_v c[6] = {mP1[0]+mP2[0], mP1[1]+mP2[1], mP1[2]+mP2[2],
                  mP1[3]+mP2[3], mP1[4]+mP2[4], mP1[5]+mP2[5]};

  float_v dx = (mP1[0]-mP2[0]);
  float_v dy = (mP1[1]-mP2[1]);
  float_v dz = (mP1[2]-mP2[2]);

  float_v l = sqrt( dx*dx + dy*dy + dz*dz );
  float_v dl = c[0]*dx*dx + c[2]*dy*dy + c[5]*dz*dz + 2*(c[1]*dx*dy + c[3]*dx*dz + c[4]*dy*dz);
  l(abs(l) < 1.e-8f) = 1.e-8f;
  dl(dl < 0.f) = 0.f;
  
  return sqrt(dl)/l;
}



void KFParticleBaseSIMD::SubtractFromVertex(  KFParticleBaseSIMD &Vtx ) const
{
  //* Subtract the particle from the vertex  

  float_v m[8];
  float_v mCm[36];

  if( Vtx.fIsLinearized ){
    GetMeasurement( Vtx.fVtxGuess, m, mCm );
  } else {
    GetMeasurement( Vtx.fP, m, mCm );
  }
  
  float_v mV[6];
    
  mV[ 0] = mCm[ 0];
  mV[ 1] = mCm[ 1];
  mV[ 2] = mCm[ 2];
  mV[ 3] = mCm[ 3];
  mV[ 4] = mCm[ 4];
  mV[ 5] = mCm[ 5];
     
  //* 

  float_v mS[6]= { mV[0] - Vtx.fC[0],
                mV[1] - Vtx.fC[1], mV[2] - Vtx.fC[2],
                mV[3] - Vtx.fC[3], mV[4] - Vtx.fC[4], mV[5] - Vtx.fC[5] };
  InvertCholetsky3(mS);
    
  //* Residual (measured - estimated)
    
  float_v zeta[3] = { m[0]-Vtx.fP[0], m[1]-Vtx.fP[1], m[2]-Vtx.fP[2] };
        
  //* mCHt = mCH' - D'
    
  float_v mCHt0[3], mCHt1[3], mCHt2[3];
    
  mCHt0[0]=Vtx.fC[ 0] ;      mCHt1[0]=Vtx.fC[ 1] ;      mCHt2[0]=Vtx.fC[ 3] ;
  mCHt0[1]=Vtx.fC[ 1] ;      mCHt1[1]=Vtx.fC[ 2] ;      mCHt2[1]=Vtx.fC[ 4] ;
  mCHt0[2]=Vtx.fC[ 3] ;      mCHt1[2]=Vtx.fC[ 4] ;      mCHt2[2]=Vtx.fC[ 5] ;
  
  //* Kalman gain K = mCH'*S
    
  float_v k0[3], k1[3], k2[3];
    
  for(Int_t i=0;i<3;++i){
    k0[i] = mCHt0[i]*mS[0] + mCHt1[i]*mS[1] + mCHt2[i]*mS[3];
    k1[i] = mCHt0[i]*mS[1] + mCHt1[i]*mS[2] + mCHt2[i]*mS[4];
    k2[i] = mCHt0[i]*mS[3] + mCHt1[i]*mS[4] + mCHt2[i]*mS[5];
  }
    
  //* New estimation of the vertex position r += K*zeta
    
  float_v dChi2 = -(mS[0]*zeta[0] + mS[1]*zeta[1] + mS[3]*zeta[2])*zeta[0]
    -      (mS[1]*zeta[0] + mS[2]*zeta[1] + mS[4]*zeta[2])*zeta[1]
    -      (mS[3]*zeta[0] + mS[4]*zeta[1] + mS[5]*zeta[2])*zeta[2];

//  float_v mask = float_v( (Vtx.fChi2 - dChi2) < 0.f ); //TODO Check and correct!
  float_v mask = float_v(1.f>0.f); //TODO Check and correct!

  for(Int_t i=0;i<3;++i) 
    Vtx.fP[i] -= (mask & (k0[i]*zeta[0] + k1[i]*zeta[1] + k2[i]*zeta[2]) );
  //* New covariance matrix C -= K*(mCH')'
    
  for(Int_t i=0, k=0;i<3;++i){
    for(Int_t j=0;j<=i;++j,++k) 
      Vtx.fC[k] += (mask & (k0[i]*mCHt0[j] + k1[i]*mCHt1[j] + k2[i]*mCHt2[j]));
  }
    
  //* Calculate Chi^2 

  Vtx.fNDF  -= 2; //TODO Check and correct!
  Vtx.fChi2 += (mask & dChi2);
}

void KFParticleBaseSIMD::SubtractFromParticle(  KFParticleBaseSIMD &Vtx ) const
{
  //* Subtract the particle from the mother particle  

  float_v m[8];
  float_v mV[36];

  if( Vtx.fIsLinearized ){
    GetMeasurement( Vtx.fVtxGuess, m, mV,0 );
  } else {
    GetMeasurement( Vtx.fP, m, mV );
  }

  float_v mS[6]= { mV[0] - Vtx.fC[0],
                mV[1] - Vtx.fC[1], mV[2] - Vtx.fC[2],
                mV[3] - Vtx.fC[3], mV[4] - Vtx.fC[4], mV[5] - Vtx.fC[5] };
  InvertCholetsky3(mS);

  //* Residual (measured - estimated)

  float_v zeta[3] = { m[0]-Vtx.fP[0], m[1]-Vtx.fP[1], m[2]-Vtx.fP[2] };    

  //* CHt = CH' - D'

  float_v mCHt0[7], mCHt1[7], mCHt2[7];

  mCHt0[0]=mV[ 0] ;           mCHt1[0]=mV[ 1] ;           mCHt2[0]=mV[ 3] ;
  mCHt0[1]=mV[ 1] ;           mCHt1[1]=mV[ 2] ;           mCHt2[1]=mV[ 4] ;
  mCHt0[2]=mV[ 3] ;           mCHt1[2]=mV[ 4] ;           mCHt2[2]=mV[ 5] ;
  mCHt0[3]=Vtx.fC[ 6]-mV[ 6]; mCHt1[3]=Vtx.fC[ 7]-mV[ 7]; mCHt2[3]=Vtx.fC[ 8]-mV[ 8];
  mCHt0[4]=Vtx.fC[10]-mV[10]; mCHt1[4]=Vtx.fC[11]-mV[11]; mCHt2[4]=Vtx.fC[12]-mV[12];
  mCHt0[5]=Vtx.fC[15]-mV[15]; mCHt1[5]=Vtx.fC[16]-mV[16]; mCHt2[5]=Vtx.fC[17]-mV[17];
  mCHt0[6]=Vtx.fC[21]-mV[21]; mCHt1[6]=Vtx.fC[22]-mV[22]; mCHt2[6]=Vtx.fC[23]-mV[23];

  //* Kalman gain K = mCH'*S
    
  float_v k0[7], k1[7], k2[7];
    
  for(Int_t i=0;i<7;++i){
    k0[i] = mCHt0[i]*mS[0] + mCHt1[i]*mS[1] + mCHt2[i]*mS[3];
    k1[i] = mCHt0[i]*mS[1] + mCHt1[i]*mS[2] + mCHt2[i]*mS[4];
    k2[i] = mCHt0[i]*mS[3] + mCHt1[i]*mS[4] + mCHt2[i]*mS[5];
  }

    //* Add the daughter momentum to the particle momentum
    
  Vtx.fP[ 3] -= m[ 3];
  Vtx.fP[ 4] -= m[ 4];
  Vtx.fP[ 5] -= m[ 5];
  Vtx.fP[ 6] -= m[ 6];
  
  Vtx.fC[ 9] -= mV[ 9];
  Vtx.fC[13] -= mV[13];
  Vtx.fC[14] -= mV[14];
  Vtx.fC[18] -= mV[18];
  Vtx.fC[19] -= mV[19];
  Vtx.fC[20] -= mV[20];
  Vtx.fC[24] -= mV[24];
  Vtx.fC[25] -= mV[25];
  Vtx.fC[26] -= mV[26];
  Vtx.fC[27] -= mV[27];

   //* New estimation of the vertex position r += K*zeta
    
  for(Int_t i=0;i<3;++i) 
    Vtx.fP[i] = m[i] - (k0[i]*zeta[0] + k1[i]*zeta[1] + k2[i]*zeta[2]);
  for(Int_t i=3;i<7;++i) 
    Vtx.fP[i] = Vtx.fP[i] - (k0[i]*zeta[0] + k1[i]*zeta[1] + k2[i]*zeta[2]);

    //* New covariance matrix C -= K*(mCH')'

  float_v ffC[28] = { -mV[ 0],
                   -mV[ 1], -mV[ 2],
                   -mV[ 3], -mV[ 4], -mV[ 5],
                    mV[ 6],  mV[ 7],  mV[ 8], Vtx.fC[ 9],
                    mV[10],  mV[11],  mV[12], Vtx.fC[13], Vtx.fC[14],
                    mV[15],  mV[16],  mV[17], Vtx.fC[18], Vtx.fC[19], Vtx.fC[20],
                    mV[21],  mV[22],  mV[23], Vtx.fC[24], Vtx.fC[25], Vtx.fC[26], Vtx.fC[27] };

  for(Int_t i=0, k=0;i<7;++i){
    for(Int_t j=0;j<=i;++j,++k){
      Vtx.fC[k] = ffC[k] + (k0[i]*mCHt0[j] + k1[i]*mCHt1[j] + k2[i]*mCHt2[j] );
    }
  }

    //* Calculate Chi^2 
  Vtx.fNDF  -= 2;
  Vtx.fQ    -= GetQ();
  Vtx.fSFromDecay = 0;    
  Vtx.fChi2 -= ((mS[0]*zeta[0] + mS[1]*zeta[1] + mS[3]*zeta[2])*zeta[0]
               +(mS[1]*zeta[0] + mS[2]*zeta[1] + mS[4]*zeta[2])*zeta[1]
               +(mS[3]*zeta[0] + mS[4]*zeta[1] + mS[5]*zeta[2])*zeta[2]);     
}

void KFParticleBaseSIMD::TransportLine( float_v dS, 
				       float_v P[], float_v C[] ) const 
{
  //* Transport the particle as a straight line

  P[0] = fP[0] + dS*fP[3];
  P[1] = fP[1] + dS*fP[4];
  P[2] = fP[2] + dS*fP[5];
  P[3] = fP[3];
  P[4] = fP[4];
  P[5] = fP[5];
  P[6] = fP[6];
  P[7] = fP[7];
 
  float_v c6  = fC[ 6] + dS*fC[ 9];
  float_v c11 = fC[11] + dS*fC[14];
  float_v c17 = fC[17] + dS*fC[20];
  float_v sc13 = dS*fC[13];
  float_v sc18 = dS*fC[18];
  float_v sc19 = dS*fC[19];

  C[ 0] = fC[ 0] + dS*( fC[ 6] + c6  );
  C[ 2] = fC[ 2] + dS*( fC[11] + c11 );
  C[ 5] = fC[ 5] + dS*( fC[17] + c17 );

  C[ 7] = fC[ 7] + sc13;
  C[ 8] = fC[ 8] + sc18;
  C[ 9] = fC[ 9];

  C[12] = fC[12] + sc19;

  C[ 1] = fC[ 1] + dS*( fC[10] + C[ 7] );
  C[ 3] = fC[ 3] + dS*( fC[15] + C[ 8] );
  C[ 4] = fC[ 4] + dS*( fC[16] + C[12] ); 
  C[ 6] = c6;

  C[10] = fC[10] + sc13;
  C[11] = c11;

  C[13] = fC[13];
  C[14] = fC[14];
  C[15] = fC[15] + sc18;
  C[16] = fC[16] + sc19;
  C[17] = c17;
  
  C[18] = fC[18];
  C[19] = fC[19];
  C[20] = fC[20];
  C[21] = fC[21] + dS*fC[24];
  C[22] = fC[22] + dS*fC[25];
  C[23] = fC[23] + dS*fC[26];

  C[24] = fC[24];
  C[25] = fC[25];
  C[26] = fC[26];
  C[27] = fC[27];
  C[28] = fC[28] + dS*fC[31];
  C[29] = fC[29] + dS*fC[32];
  C[30] = fC[30] + dS*fC[33];

  C[31] = fC[31];
  C[32] = fC[32];
  C[33] = fC[33];
  C[34] = fC[34];
  C[35] = fC[35]; 
}

void KFParticleBaseSIMD::ConstructGammaBz( const KFParticleBaseSIMD &daughter1,
                                           const KFParticleBaseSIMD &daughter2, float_v Bz  )
{ 
  //* Create gamma
  
  const KFParticleBaseSIMD *daughters[2] = { &daughter1, &daughter2};

  float_v v0[3];
  
  if( !fIsLinearized ){
    float_v ds, ds1;
    float_v m[8];
    float_v mCd[36];       
    daughter1.GetDStoParticle(daughter2, ds, ds1);      
    daughter1.Transport( ds, m, mCd );
    fP[0] = m[0];
    fP[1] = m[1];
    fP[2] = m[2];
    daughter2.Transport( ds1, m, mCd );
    fP[0] = .5f*( fP[0] + m[0] );
    fP[1] = .5f*( fP[1] + m[1] );
    fP[2] = .5f*( fP[2] + m[2] );
  } else {
    fP[0] = fVtxGuess[0];
    fP[1] = fVtxGuess[1];
    fP[2] = fVtxGuess[2];
  }

  float_v daughterP[2][8], daughterC[2][36];
  float_v vtxMom[2][3];

  int nIter = fIsLinearized ?1 :2;

  for( int iter=0; iter<nIter; iter++){

    v0[0] = fP[0];
    v0[1] = fP[1];
    v0[2] = fP[2];
    
    fAtProductionVertex = 0;
    fSFromDecay = 0;
    fP[0] = v0[0];
    fP[1] = v0[1];
    fP[2] = v0[2];
    fP[3] = 0;
    fP[4] = 0;
    fP[5] = 0;
    fP[6] = 0;
    fP[7] = 0;

  
    // fit daughters to the vertex guess  
    
    {  
      for( int id=0; id<2; id++ ){
	
	float_v *p = daughterP[id];
	float_v *mC = daughterC[id];
	
	daughters[id]->GetMeasurement( v0, p, mC );
	
	float_v mAi[6] = {mC[0], mC[1], mC[2], mC[3], mC[4], mC[5]};
	InvertCholetsky3( mAi );
	
	float_v mB[3][3];
	
	mB[0][0] = mC[ 6]*mAi[0] + mC[ 7]*mAi[1] + mC[ 8]*mAi[3];
	mB[0][1] = mC[ 6]*mAi[1] + mC[ 7]*mAi[2] + mC[ 8]*mAi[4];
	mB[0][2] = mC[ 6]*mAi[3] + mC[ 7]*mAi[4] + mC[ 8]*mAi[5];
	
	mB[1][0] = mC[10]*mAi[0] + mC[11]*mAi[1] + mC[12]*mAi[3];
	mB[1][1] = mC[10]*mAi[1] + mC[11]*mAi[2] + mC[12]*mAi[4];
	mB[1][2] = mC[10]*mAi[3] + mC[11]*mAi[4] + mC[12]*mAi[5];
	
	mB[2][0] = mC[15]*mAi[0] + mC[16]*mAi[1] + mC[17]*mAi[3];
	mB[2][1] = mC[15]*mAi[1] + mC[16]*mAi[2] + mC[17]*mAi[4];
	mB[2][2] = mC[15]*mAi[3] + mC[16]*mAi[4] + mC[17]*mAi[5];
	
	float_v z[3] = { v0[0]-p[0], v0[1]-p[1], v0[2]-p[2] };
	
	vtxMom[id][0] = p[3] + mB[0][0]*z[0] + mB[0][1]*z[1] + mB[0][2]*z[2];
	vtxMom[id][1] = p[4] + mB[1][0]*z[0] + mB[1][1]*z[1] + mB[1][2]*z[2];
	vtxMom[id][2] = p[5] + mB[2][0]*z[0] + mB[2][1]*z[1] + mB[2][2]*z[2];
	
	daughters[id]->Transport( daughters[id]->GetDStoPoint(v0), p, mC );
      
      }      
      
    } // fit daughters to guess

    
    // fit new vertex
    {
      
      float_v mpx0 =  vtxMom[0][0]+vtxMom[1][0];
      float_v mpy0 =  vtxMom[0][1]+vtxMom[1][1];
      float_v mpt0 =  sqrt(mpx0*mpx0 + mpy0*mpy0);
      // float_v a0 = TMath::ATan2(mpy0,mpx0);
      
      float_v ca0 = mpx0/mpt0;
      float_v sa0 = mpy0/mpt0;
      float_v r[3] = { v0[0], v0[1], v0[2] };
      float_v mC[3][3] = {{1000.f, 0.f ,   0.f  },
			 {0.f,  1000.f,   0.f  },
			 {0.f,     0.f, 1000.f } };
      float_v chi2=0.f;
      
      for( int id=0; id<2; id++ ){		
	const float_v kCLight = 0.000299792458f;
	float_v q = Bz*daughters[id]->GetQ()*kCLight;
	float_v px0 = vtxMom[id][0];
	float_v py0 = vtxMom[id][1];
	float_v pz0 = vtxMom[id][2];
	float_v pt0 = sqrt(px0*px0+py0*py0);
	float_v mG[3][6], mB[3], mH[3][3];
	// r = {vx,vy,vz};
	// m = {x,y,z,Px,Py,Pz};
	// V = daughter.C
	// G*m + B = H*r;
	// q*x + Py - q*vx - sin(a)*Pt = 0
	// q*y - Px - q*vy + cos(a)*Pt = 0
	// (Px*cos(a) + Py*sin(a) ) (vz -z) - Pz( cos(a)*(vx-x) + sin(a)*(vy-y)) = 0
	
	mG[0][0] = q;
	mG[0][1] = 0.f;
	mG[0][2] = 0.f;
	mG[0][3] =   -sa0*px0/pt0;
	mG[0][4] = 1.f -sa0*py0/pt0;
	mG[0][5] = 0.f;	
	mH[0][0] = q;
	mH[0][1] = 0.f;
	mH[0][2] = 0.f;      
	mB[0] = py0 - sa0*pt0 - mG[0][3]*px0 - mG[0][4]*py0 ;
	
	// q*y - Px - q*vy + cos(a)*Pt = 0
	
	mG[1][0] = 0.f;
	mG[1][1] = q;
	mG[1][2] = 0.f;
	mG[1][3] = -1.f + ca0*px0/pt0;
	mG[1][4] =    + ca0*py0/pt0;
	mG[1][5] = 0.f;      
	mH[1][0] = 0.f;
	mH[1][1] = q;
	mH[1][2] = 0.f;      
	mB[1] = -px0 + ca0*pt0 - mG[1][3]*px0 - mG[1][4]*py0 ;
	
	// (Px*cos(a) + Py*sin(a) ) (z -vz) - Pz( cos(a)*(x-vx) +sin(a)*(y-vy)) = 0
      
	mG[2][0] = -pz0*ca0;
	mG[2][1] = -pz0*sa0;
	mG[2][2] =  px0*ca0 + py0*sa0;
	mG[2][3] = 0.f;
	mG[2][4] = 0.f;
	mG[2][5] = 0.f;
	
	mH[2][0] = mG[2][0];
	mH[2][1] = mG[2][1];
	mH[2][2] = mG[2][2];
	
	mB[2] = 0.f;
	
	// fit the vertex

	// V = GVGt

	float_v mGV[3][6];
	float_v mV[6];
	float_v m[3];
	for( int i=0; i<3; i++ ){
	  m[i] = mB[i];
	  for( int k=0; k<6; k++ ) m[i]+=mG[i][k]*daughterP[id][k];
	}
	for( int i=0; i<3; i++ ){
	  for( int j=0; j<6; j++ ){
	    mGV[i][j] = 0;
	    for( int k=0; k<6; k++ ) mGV[i][j]+=mG[i][k]*daughterC[id][ IJ(k,j) ];
	  }
	}
	for( int i=0, k=0; i<3; i++ ){
	  for( int j=0; j<=i; j++,k++ ){
	    mV[k] = 0;
	    for( int l=0; l<6; l++ ) mV[k]+=mGV[i][l]*mG[j][l];
	  }
	}
	
      
	//* CHt
	
	float_v mCHt[3][3];
	float_v mHCHt[6];
	float_v mHr[3];
	for( int i=0; i<3; i++ ){	  
	  mHr[i] = 0;
	  for( int k=0; k<3; k++ ) mHr[i]+= mH[i][k]*r[k];
	}
      
	for( int i=0; i<3; i++ ){
	  for( int j=0; j<3; j++){
	    mCHt[i][j] = 0;
	    for( int k=0; k<3; k++ ) mCHt[i][j]+= mC[i][k]*mH[j][k];
	  }
	}

	for( int i=0, k=0; i<3; i++ ){
	  for( int j=0; j<=i; j++, k++ ){
	    mHCHt[k] = 0;
	    for( int l=0; l<3; l++ ) mHCHt[k]+= mH[i][l]*mCHt[l][j];
	  }
	}
      
	float_v mS[6] = { mHCHt[0]+mV[0], 
			   mHCHt[1]+mV[1], mHCHt[2]+mV[2], 
			   mHCHt[3]+mV[3], mHCHt[4]+mV[4], mHCHt[5]+mV[5]    };	
      

	InvertCholetsky3(mS);
	
	//* Residual (measured - estimated)
    
	float_v zeta[3] = { m[0]-mHr[0], m[1]-mHr[1], m[2]-mHr[2] };
            
	//* Kalman gain K = mCH'*S
    
	float_v k[3][3];
      
	for(Int_t i=0;i<3;++i){
	  k[i][0] = mCHt[i][0]*mS[0] + mCHt[i][1]*mS[1] + mCHt[i][2]*mS[3];
	  k[i][1] = mCHt[i][0]*mS[1] + mCHt[i][1]*mS[2] + mCHt[i][2]*mS[4];
	  k[i][2] = mCHt[i][0]*mS[3] + mCHt[i][1]*mS[4] + mCHt[i][2]*mS[5];
	}

	//* New estimation of the vertex position r += K*zeta
    
	for(Int_t i=0;i<3;++i) 
	  r[i] = r[i] + k[i][0]*zeta[0] + k[i][1]*zeta[1] + k[i][2]*zeta[2];
      
	//* New covariance matrix C -= K*(mCH')'

	for(Int_t i=0;i<3;++i){
	  for(Int_t j=0;j<=i;++j){
	    mC[i][j] = mC[i][j] - (k[i][0]*mCHt[j][0] + k[i][1]*mCHt[j][1] + k[i][2]*mCHt[j][2]);
	    mC[j][i] = mC[i][j];
	  }
	}

	//* Calculate Chi^2 
	
	chi2 += ( ( mS[0]*zeta[0] + mS[1]*zeta[1] + mS[3]*zeta[2] )*zeta[0]
		  +(mS[1]*zeta[0] + mS[2]*zeta[1] + mS[4]*zeta[2] )*zeta[1]
		  +(mS[3]*zeta[0] + mS[4]*zeta[1] + mS[5]*zeta[2] )*zeta[2]  );  
      }
    
      // store vertex
    
      fNDF  = 2;
      fChi2 = chi2;
      for( int i=0; i<3; i++ ) fP[i] = r[i];
      for( int i=0,k=0; i<3; i++ ){
	for( int j=0; j<=i; j++,k++ ){
	  fC[k] = mC[i][j];
	}
      }
    }

  } // iterations

  // now fit daughters to the vertex
  
  fQ     =  0.f;
  fSFromDecay = 0.f;    

  for(Int_t i=3;i<8;++i) fP[i]=0.f;
  for(Int_t i=6;i<35;++i) fC[i]=0.f;
  fC[35] = 100.f;

  for( int id=0; id<2; id++ ){

    float_v *p = daughterP[id];
    float_v *mC = daughterC[id];      
    daughters[id]->GetMeasurement( v0, p, mC );

    const float_v *m = fP, *mV = fC;
    
    float_v mAi[6] = {mC[0], mC[1], mC[2], mC[3], mC[4], mC[5]};
    InvertCholetsky3( mAi );

    float_v mB[4][3];

    mB[0][0] = mC[ 6]*mAi[0] + mC[ 7]*mAi[1] + mC[ 8]*mAi[3];
    mB[0][1] = mC[ 6]*mAi[1] + mC[ 7]*mAi[2] + mC[ 8]*mAi[4];
    mB[0][2] = mC[ 6]*mAi[3] + mC[ 7]*mAi[4] + mC[ 8]*mAi[5];
    
    mB[1][0] = mC[10]*mAi[0] + mC[11]*mAi[1] + mC[12]*mAi[3];
    mB[1][1] = mC[10]*mAi[1] + mC[11]*mAi[2] + mC[12]*mAi[4];
    mB[1][2] = mC[10]*mAi[3] + mC[11]*mAi[4] + mC[12]*mAi[5];
    
    mB[2][0] = mC[15]*mAi[0] + mC[16]*mAi[1] + mC[17]*mAi[3];
    mB[2][1] = mC[15]*mAi[1] + mC[16]*mAi[2] + mC[17]*mAi[4];
    mB[2][2] = mC[15]*mAi[3] + mC[16]*mAi[4] + mC[17]*mAi[5];
    
    mB[3][0] = mC[21]*mAi[0] + mC[22]*mAi[1] + mC[23]*mAi[3];
    mB[3][1] = mC[21]*mAi[1] + mC[22]*mAi[2] + mC[23]*mAi[4];
    mB[3][2] = mC[21]*mAi[3] + mC[22]*mAi[4] + mC[23]*mAi[5];    


    float_v z[3] = { m[0]-p[0], m[1]-p[1], m[2]-p[2] };

//     {
//       float_v mAV[6] = { mC[0]-mV[0], mC[1]-mV[1], mC[2]-mV[2], 
// 			  mC[3]-mV[3], mC[4]-mV[4], mC[5]-mV[5] };
//       
//       float_v mAVi[6];
//       if( !InvertSym3(mAV, mAVi) ){
// 	float_v dChi2 = ( +(mAVi[0]*z[0] + mAVi[1]*z[1] + mAVi[3]*z[2])*z[0]
// 			   +(mAVi[1]*z[0] + mAVi[2]*z[1] + mAVi[4]*z[2])*z[1]
// 			   +(mAVi[3]*z[0] + mAVi[4]*z[1] + mAVi[5]*z[2])*z[2] );
// 	fChi2+= TMath::Abs( dChi2 );
//       }
//       fNDF  += 2;
//     }

    //* Add the daughter momentum to the particle momentum
 
    fP[3]+= p[3] + mB[0][0]*z[0] + mB[0][1]*z[1] + mB[0][2]*z[2];
    fP[4]+= p[4] + mB[1][0]*z[0] + mB[1][1]*z[1] + mB[1][2]*z[2];
    fP[5]+= p[5] + mB[2][0]*z[0] + mB[2][1]*z[1] + mB[2][2]*z[2];
    fP[6]+= p[6] + mB[3][0]*z[0] + mB[3][1]*z[1] + mB[3][2]*z[2];
  
    float_v d0, d1, d2;
   
    d0= mB[0][0]*mV[0] + mB[0][1]*mV[1] + mB[0][2]*mV[3] - mC[ 6];
    d1= mB[0][0]*mV[1] + mB[0][1]*mV[2] + mB[0][2]*mV[4] - mC[ 7];
    d2= mB[0][0]*mV[3] + mB[0][1]*mV[4] + mB[0][2]*mV[5] - mC[ 8];

    //fC[6]+= mC[ 6] + d0;
    //fC[7]+= mC[ 7] + d1;
    //fC[8]+= mC[ 8] + d2;
    fC[9]+= mC[ 9] + d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];

    d0= mB[1][0]*mV[0] + mB[1][1]*mV[1] + mB[1][2]*mV[3] - mC[10];
    d1= mB[1][0]*mV[1] + mB[1][1]*mV[2] + mB[1][2]*mV[4] - mC[11];
    d2= mB[1][0]*mV[3] + mB[1][1]*mV[4] + mB[1][2]*mV[5] - mC[12];

    //fC[10]+= mC[10]+ d0;
    //fC[11]+= mC[11]+ d1;
    //fC[12]+= mC[12]+ d2;
    fC[13]+= mC[13]+ d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];
    fC[14]+= mC[14]+ d0*mB[1][0] + d1*mB[1][1] + d2*mB[1][2];

    d0= mB[2][0]*mV[0] + mB[2][1]*mV[1] + mB[2][2]*mV[3] - mC[15];
    d1= mB[2][0]*mV[1] + mB[2][1]*mV[2] + mB[2][2]*mV[4] - mC[16];
    d2= mB[2][0]*mV[3] + mB[2][1]*mV[4] + mB[2][2]*mV[5] - mC[17];

    //fC[15]+= mC[15]+ d0;
    //fC[16]+= mC[16]+ d1;
    //fC[17]+= mC[17]+ d2;
    fC[18]+= mC[18]+ d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];
    fC[19]+= mC[19]+ d0*mB[1][0] + d1*mB[1][1] + d2*mB[1][2];
    fC[20]+= mC[20]+ d0*mB[2][0] + d1*mB[2][1] + d2*mB[2][2];

    d0= mB[3][0]*mV[0] + mB[3][1]*mV[1] + mB[3][2]*mV[3] - mC[21];
    d1= mB[3][0]*mV[1] + mB[3][1]*mV[2] + mB[3][2]*mV[4] - mC[22];
    d2= mB[3][0]*mV[3] + mB[3][1]*mV[4] + mB[3][2]*mV[5] - mC[23];

    //fC[21]+= mC[21] + d0;
    //fC[22]+= mC[22] + d1;
    //fC[23]+= mC[23] + d2;
    fC[24]+= mC[24] + d0*mB[0][0] + d1*mB[0][1] + d2*mB[0][2];
    fC[25]+= mC[25] + d0*mB[1][0] + d1*mB[1][1] + d2*mB[1][2];
    fC[26]+= mC[26] + d0*mB[2][0] + d1*mB[2][1] + d2*mB[2][2];
    fC[27]+= mC[27] + d0*mB[3][0] + d1*mB[3][1] + d2*mB[3][2];
  }

//  SetMassConstraint(0,0);
  SetNonlinearMassConstraint(0.f);
}

void KFParticleBaseSIMD::GetArmenterosPodolanski(KFParticleBaseSIMD& positive, KFParticleBaseSIMD& negative, float_v QtAlfa[2] )
{
// example:
//       KFParticle PosParticle(...)
//       KFParticle NegParticle(...)
//       Gamma.ConstructGamma(PosParticle, NegParticle);
//       float_v VertexGamma[3] = {Gamma.GetX(), Gamma.GetY(), Gamma.GetZ()};
//       PosParticle.TransportToPoint(VertexGamma);
//       NegParticle.TransportToPoint(VertexGamma);
//       float_v armenterosQtAlfa[2] = {0.};
//       KFParticle::GetArmenterosPodolanski(PosParticle, NegParticle, armenterosQtAlfa );

  float_v alpha = 0.f, qt = 0.f;
  float_v spx = positive.GetPx() + negative.GetPx();
  float_v spy = positive.GetPy() + negative.GetPy();
  float_v spz = positive.GetPz() + negative.GetPz();
  float_v sp  = sqrt(spx*spx + spy*spy + spz*spz);
  float_v mask = float_v(  abs(sp) < float_v(1.E-10f));
  float_v pn, pp, pln, plp;

  pn = sqrt(negative.GetPx()*negative.GetPx() + negative.GetPy()*negative.GetPy() + negative.GetPz()*negative.GetPz());
  pp = sqrt(positive.GetPx()*positive.GetPx() + positive.GetPy()*positive.GetPy() + positive.GetPz()*positive.GetPz());
  pln  = (negative.GetPx()*spx+negative.GetPy()*spy+negative.GetPz()*spz)/sp;
  plp  = (positive.GetPx()*spx+positive.GetPy()*spy+positive.GetPz()*spz)/sp;

  mask = float_v(mask & float_v(  abs(pn) < float_v(1.E-10f)));
  float_v ptm  = (1.f-((pln/pn)*(pln/pn)));
  qt(ptm >= 0.f) =  pn*sqrt(ptm);
  alpha = (plp-pln)/(plp+pln);

  QtAlfa[0] = float_v(qt & mask);
  QtAlfa[1] = float_v(alpha & mask);
}

void KFParticleBaseSIMD::RotateXY(float_v angle, float_v Vtx[3])
{
  // Rotates the KFParticle object around OZ axis, OZ axis is set by the vertex position
  // float_v angle - angle of rotation in XY plane in [rad]
  // float_v Vtx[3] - position of the vertex in [cm]

  // Before rotation the center of the coordinat system should be moved to the vertex position; move back after rotation
  X() = X() - Vtx[0];
  Y() = Y() - Vtx[1];
  Z() = Z() - Vtx[2];

  // Rotate the kf particle
  float_v s = KFPMath::Sin(angle);
  float_v c = KFPMath::Cos(angle);
  
  float_v mA[8][ 8];
  for( Int_t i=0; i<8; i++ ){
    for( Int_t j=0; j<8; j++){
      mA[i][j] = 0;
    }
  }
  for( int i=0; i<8; i++ ){
    mA[i][i] = 1;
  }
  mA[0][0] =  c;  mA[0][1] = s;
  mA[1][0] = -s;  mA[1][1] = c;
  mA[3][3] =  c;  mA[3][4] = s;
  mA[4][3] = -s;  mA[4][4] = c;

  float_v mAC[8][8];
  float_v mAp[8];

  for( Int_t i=0; i<8; i++ ){
    mAp[i] = 0;
    for( Int_t k=0; k<8; k++){
      mAp[i]+=mA[i][k] * fP[k];
    }
  }

  for( Int_t i=0; i<8; i++){
    fP[i] = mAp[i];
  }

  for( Int_t i=0; i<8; i++ ){
    for( Int_t j=0; j<8; j++ ){
      mAC[i][j] = 0;
      for( Int_t k=0; k<8; k++ ){
        mAC[i][j]+= mA[i][k] * GetCovariance(k,j);
      }
    }
  }

  for( Int_t i=0; i<8; i++ ){
    for( Int_t j=0; j<=i; j++ ){
      float_v xx = 0.f;
      for( Int_t k=0; k<8; k++){
        xx+= mAC[i][k]*mA[j][k];
      }
      Covariance(i,j) = xx;
    }
  }

  X() = GetX() + Vtx[0];
  Y() = GetY() + Vtx[1];
  Z() = GetZ() + Vtx[2];
}

void KFParticleBaseSIMD::InvertCholetsky3(float_v a[6])
{
//a[3] = a[3]/10;
//a[4] = a[4]/10;
//a[5] = a[5]/10;
// float_v ai[6] = {a[0], a[1], a[2], a[3], a[4], a[5]};

  float_v d[3], uud, u[3][3];
  for(int i=0; i<3; i++) 
  {
    d[i]=0.f;
    for(int j=0; j<3; j++) 
      u[i][j]=0.f;
  }

  for(int i=0; i<3 ; i++)
  {
    uud=0.f;
    for(int j=0; j<i; j++) 
      uud += u[j][i]*u[j][i]*d[j];
    uud = a[i*(i+3)/2] - uud;

    float_v smallval = 1.e-12f;
    uud( abs(uud)<smallval ) = smallval;

    uud(abs(uud)<1.e-8f) = 1.e-8f;
    d[i] = uud/abs(uud);
    u[i][i] = sqrt(abs(uud));

    for(int j=i+1; j<3; j++) 
    {
      uud = 0.f;
      for(int k=0; k<i; k++)
        uud += u[k][i]*u[k][j]*d[k];
      uud = a[j*(j+1)/2+i] - uud;
      u[i][j] = d[i]/u[i][i]*uud;
    }
  }

  float_v u1[3];

  for(int i=0; i<3; i++)
  {
    u1[i] = u[i][i];
    u[i][i] = 1.f/u[i][i];
  }
  for(int i=0; i<2; i++)
  {
    u[i][i+1] = - u[i][i+1]*u[i][i]*u[i+1][i+1];
  }
  for(int i=0; i<1; i++)
  {
    u[i][i+2] = u[i][i+1]*u1[i+1]*u[i+1][i+2]-u[i][i+2]*u[i][i]*u[i+2][i+2];
  }

  for(int i=0; i<3; i++)
    a[i+3] = u[i][2]*u[2][2]*d[2];
  for(int i=0; i<2; i++)
    a[i+1] = u[i][1]*u[1][1]*d[1] + u[i][2]*u[1][2]*d[2];
  a[0] = u[0][0]*u[0][0]*d[0] + u[0][1]*u[0][1]*d[1] + u[0][2]*u[0][2]*d[2];

// float_v mI[9];
// mI[0] = a[0]*ai[0] + a[1]*ai[1] + a[3]*ai[3];
// mI[1] = a[0]*ai[1] + a[1]*ai[2] + a[3]*ai[4];
// mI[2] = a[0]*ai[3] + a[1]*ai[4] + a[3]*ai[5];
// 
// mI[3] = a[1]*ai[0] + a[2]*ai[1] + a[4]*ai[3];
// mI[4] = a[1]*ai[1] + a[2]*ai[2] + a[4]*ai[4];
// mI[5] = a[1]*ai[3] + a[2]*ai[4] + a[4]*ai[5];
// 
// mI[6] = a[3]*ai[0] + a[4]*ai[1] + a[5]*ai[3];
// mI[7] = a[3]*ai[1] + a[4]*ai[2] + a[5]*ai[4];
// mI[8] = a[3]*ai[3] + a[4]*ai[4] + a[5]*ai[5];
// 
// 
// std::cout << "In Matrix"<<std::endl;
// std::cout << ai[0][0] << " " << ai[1][0] << " " << ai[3][0] << std::endl;
// std::cout << ai[1][0] << " " << ai[2][0] << " " << ai[4][0] << std::endl;
// std::cout << ai[3][0] << " " << ai[4][0] << " " << ai[5][0] << std::endl;
// std::cout << "I"<<std::endl;
// std::cout << mI[0][0] << " " << mI[1][0] << " " << mI[2][0] << std::endl;
// std::cout << mI[3][0] << " " << mI[4][0] << " " << mI[5][0] << std::endl;
// std::cout << mI[6][0] << " " << mI[7][0] << " " << mI[8][0] << std::endl;
// std::cout << " " <<std::endl;
// std::cout << (ai[0][0]/ai[1][0]) << " "<< (ai[1][0]/ai[2][0]) << " " << (ai[3][0]/ai[4][0]) <<std::endl;
// std::cout << (ai[0][0]/ai[3][0]) << " "<< (ai[1][0]/ai[4][0]) << " " << (ai[3][0]/ai[5][0]) <<std::endl;
// 
// int ui;
// std::cin >>ui;

}

void KFParticleBaseSIMD::MultQSQt( const float_v Q[], const float_v S[], float_v SOut[] )
{
  //* Matrix multiplication Q*S*Q^T, Q - square matrix, S - symmetric

  const Int_t kN= 8;
  float_v mA[kN*kN];
  
  for( Int_t i=0, ij=0; i<kN; i++ ){
    for( Int_t j=0; j<kN; j++, ++ij ){
      mA[ij] = 0 ;
      for( Int_t k=0; k<kN; ++k ) mA[ij]+= S[( k<=i ) ? i*(i+1)/2+k :k*(k+1)/2+i] * Q[ j*kN+k];
    }
  }
    
  for( Int_t i=0; i<kN; i++ ){
    for( Int_t j=0; j<=i; j++ ){
      Int_t ij = ( j<=i ) ? i*(i+1)/2+j :j*(j+1)/2+i;
      SOut[ij] = 0 ;
      for( Int_t k=0; k<kN; k++ )  SOut[ij] += Q[ i*kN+k ] * mA[ k*kN+j ];
    }
  }
}


// 72-charachters line to define the printer border
//3456789012345678901234567890123456789012345678901234567890123456789012


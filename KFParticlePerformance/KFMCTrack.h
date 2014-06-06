//----------------------------------------------------------------------------
// Implementation of the KFParticle class
// .
// @author  I.Kisel, I.Kulakov, M.Zyzak
// @version 1.0
// @since   20.08.13
// 
// 
//  -= Copyright &copy ALICE HLT and CBM L1 Groups =-
//____________________________________________________________________________

#ifndef KFMCTRACK_H
#define KFMCTRACK_H

#include <iostream>
#include <vector>
using std::ostream;
using std::istream;


/**
 * @class KFMCTrack
 * store MC track information for Performance
 */
class KFMCTrack
{
 public:
  KFMCTrack():fMotherId(0),fPDG(0),fP(0.f),fPt(0.f),fNMCPoints(0),fIsReconstructed(0) {};

  int MotherId()          const { return fMotherId; }
  int    PDG()            const { return fPDG;}
  float Par( int i )      const { return fPar[i]; }
  float X()               const { return fPar[0]; }
  float Y()               const { return fPar[1]; }
  float Z()               const { return fPar[2]; }
  float Px()              const { return fPar[3]*fP; }
  float Py()              const { return fPar[4]*fP; }
  float Pz()              const { return fPar[5]*fP; }
  float P()               const { return fP; }
  float Pt()              const { return fPt; }
  const float *Par()      const { return fPar; }
  int   NMCPoints()       const { return fNMCPoints; }
  bool  IsReconstructed() const { return fIsReconstructed; }
  
  void SetPar( int i, float v )  { fPar[i] = v; }
  void SetX( float v )           { fPar[0] = v; }
  void SetY( float v )           { fPar[1] = v; }
  void SetZ( float v )           { fPar[2] = v; }
  void SetPx( float v )          { fPar[3] = v; }
  void SetPy( float v )          { fPar[4] = v; }
  void SetPz( float v )          { fPar[5] = v; }
  void SetQP( float v )          { fPar[6] = v; }
  void SetMotherId( int v )      { fMotherId = v; }
  void SetP ( float v )          { fP = v; }
  void SetPt( float v )          { fPt = v; }
  void SetPDG( int v )           { fPDG = v; }
  void SetNMCPoints( int v )     { fNMCPoints = v; }
  void SetReconstructed()        { fIsReconstructed = 1; }
  
 protected:

  int   fMotherId;      //* index of mother track in tracks array. -1 for primary tracks. -2 if a mother track is not in the acceptance
  int   fPDG;           //* particle pdg code
  float fPar[7];        //* x,y,z,ex,ey,ez,q/p
  float fP, fPt;        //* momentum and transverse momentum
  int   fNMCPoints;     //* N MC points
  
  bool fIsReconstructed;
};

#endif

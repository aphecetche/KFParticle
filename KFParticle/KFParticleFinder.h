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


#ifndef KFParticleFinder_h
#define KFParticleFinder_h

#include "KFParticle.h"
#include "KFParticleSIMD.h"

#include "KFPTrackVector.h"

#include <vector>

#ifdef NonhomogeniousField
class L1FieldRegion;
#endif

class KFParticleFinder
{
 public:

  KFParticleFinder();
  ~KFParticleFinder() {};
  
  void SetNThreads(short int n) { fNThreads = n;}

  /// Find particles with 2-body decay channel from input tracks vRTracks with primary vertex PrimVtx:
  /// 1. K0s->pi+ pi-
  /// 2. Lambda->p pi-
  /// All particles are put into the Particles array. 3 cuts for each particle are required.
  /// First index in the cuts array sets a particle number (see table above), second index - a cut number:
  /// cut[0][0] - chi to a primary vertex of a track (sqare root from a normalized on a total error of 
  ///             the track and the vertex distance between the track and the primary vertex), only
  ///             element cut[0][0] is used to select tracks, all other elements cut[*][0] are not used;
  /// cut[*][1] - chi2/ndf of the reconstructed particle;
  /// cut[*][2] - z coordinate of the reconstructed particle.
  /// cut[*][3] - chi2/ndf of the reconstructed particle fitted to the PV;

     void FindParticles(KFPTrackVector* vRTracks, kfvector_float* ChiToPrimVtx,
#ifdef NonhomogeniousField
                        std::vector<L1FieldRegion>& vField,
#endif
                        std::vector<KFParticle>& Particles, KFParticleSIMD* PrimVtx, int nPV);

     void ExtrapolateToPV(std::vector<KFParticle>& vParticles, KFParticleSIMD& PrimVtx);
     float_v GetChi2BetweenParticles(KFParticleSIMD &p1, KFParticleSIMD &p2);
     
    inline void ConstructV0(KFPTrackVector* vTracks,
                     int iTrTypePos,
                     int iTrTypeNeg,
#ifdef NonhomogeniousField
                     const std::vector<L1FieldRegion>& vField,
#endif
                     uint_v& idPosDaughters,
                     uint_v& idNegDaughters,
                     int_v& daughterPosPDG,
                     int_v& daughterNegPDG,
                     float_v& dS,
                     KFParticleSIMD& mother,
                     KFParticle& mother_temp,
                     const unsigned short NTracks,
                     std::vector<float_v>& l,
                     std::vector<float_v>& dl,
                     std::vector<KFParticle>& Particles,
                     KFParticleSIMD* PrimVtx,
                     const float* cuts,
                     const int_v& pvIndex,
                     const float* secCuts,
                     const float_v& massMotherPDG,
                     const float_v& massMotherPDGSigma,
                     KFParticleSIMD& motherPrimSecCand,
                     int& nPrimSecCand,
                     std::vector< std::vector<KFParticle> >* vMotherPrim = 0,
                     std::vector<KFParticle>* vMotherSec = 0
                    ) __attribute__((always_inline));
    
    void SaveV0PrimSecCand(KFParticleSIMD& mother,
                           int& NParticles,
                           KFParticle& mother_temp,
                           KFParticleSIMD* PrimVtx,
                           const float* secCuts,
                           std::vector< std::vector<KFParticle> >* vMotherPrim,
                           std::vector<KFParticle>* vMotherSec);
    
    void ConstructTrackV0Cand(KFPTrackVector& vTracks,
#ifdef NonhomogeniousField
                               const std::vector<L1FieldRegion>& vField,
#endif
                               uint_v& idTracks,
                               int_v& trackPDG,
                               KFParticle* vV0[],
                               float_v& dS,
                               KFParticleSIMD& mother,
                               KFParticleSIMD* motherTopo,
                               KFParticle& mother_temp,
                               const unsigned short nElements,
                               std::vector<float_v>& l,
                               std::vector<float_v>& dl,
                               std::vector<KFParticle>& Particles,
                               KFParticleSIMD* PrimVtx,
                               const float_v* cuts,
                               const int_v& pvIndex,
                               const float_v& massMotherPDG,
                               const float_v& massMotherPDGSigma,
                               std::vector< std::vector<KFParticle> >* vMotherPrim,
                               std::vector<KFParticle>* vMotherSec);

    void Find2DaughterDecay(KFPTrackVector* vTracks, kfvector_float* ChiToPrimVtx,
#ifdef NonhomogeniousField
                            const std::vector<L1FieldRegion>& vField,
#endif
                            std::vector<KFParticle>& Particles,
                            KFParticleSIMD* PrimVtx,
                            const float* cuts,
                            const float* secCuts,
                            std::vector< std::vector<KFParticle> >* vMotherPrim,
                            std::vector<KFParticle>* vMotherSec );

  void FindTrackV0Decay(std::vector<KFParticle>& vV0,
                        const int V0PDG,
                        KFPTrackVector& vTracks,
                        const int q,
                        const int firstTrack,
                        const int lastTrack,
#ifdef NonhomogeniousField
                        const std::vector<L1FieldRegion>& vField,
#endif

                        std::vector<KFParticle>& Particles,    
                        KFParticleSIMD* PrimVtx,
                        int v0PVIndex = -1,
                        kfvector_float* ChiToPrimVtx = 0,
                        std::vector< std::vector<KFParticle> >* vMotherPrim = 0,
                        std::vector<KFParticle>* vMotherSec = 0);

  void SelectParticles(std::vector<KFParticle>& Particles,
                       std::vector<KFParticle>& vCandidates,
                       KFParticleSIMD* PrimVtx,
                       const float& cutChi2Topo,
                       const float& cutLdL);
  
  void CombinePartPart(std::vector<KFParticle>& particles1,
                       std::vector<KFParticle>& particles2,
                       std::vector<KFParticle>& Particles,
                       KFParticleSIMD* PrimVtx,
                       const float* cuts,
                       int iPV,
                       const int MotherPDG,
                       bool isSameInputPart = 0,
                       bool saveOnlyPrimary = 1,
                       std::vector< std::vector<KFParticle> >* vMotherPrim = 0,
                       std::vector<KFParticle>* vMotherSec = 0,
                       float massMotherPDG = 0.f,
                       float massMotherPDGSigma = 0.f);
 private:

  short int fNPV;
  short int fNThreads;
  
  float fCuts2D[3];
  float fSecCuts[3];
  float fCutsTrackV0[6][3];
  float fCutsPartPart[8][3];
  
  //cuts on charm particles
  float fCutCharmPt, fCutCharmChiPrim; //cuts on tracks
  float fCutsCharm[8][3]; //cuts on reconstructed particles
  
  //cuts on LVM
  float fCutLVMPt, fCutLVMP;
  
  //cuts on J/Psi
  float fCutJPsiPt;
  
  //vectors with temporary particles for charm reconstruction
  std::vector<KFParticle> fD0;
  std::vector<KFParticle> fD0bar;
  std::vector<KFParticle> fD04;
  std::vector<KFParticle> fD04bar;
  std::vector<KFParticle> fDPlus;
  std::vector<KFParticle> fDMinus;
  //vectors with temporary particles for H0
  std::vector<KFParticle> fLPi; //Lambda Pi+ combination
  std::vector<int> fLPiPIndex; //index of the proton in Labmda
};

#endif /* !KFParticleFinder_h */


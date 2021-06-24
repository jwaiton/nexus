// -----------------------------------------------------------------------------
// nexus | NextDemoSiPMBoard.cc
//
// Geometry of the DEMO++ SiPM board.
// It consists of an 8x8 array of SensL SiPMs on a kapton board.
// The board can be covered with a teflon mask, or not.
// The teflon mask may have membranes covering the holes, or not.
// The teflon mask may be coated with TPB or not.
//
// The NEXT Collaboration
// -----------------------------------------------------------------------------


#include "NextDemoSiPMBoard.h"

#include "MaterialsList.h"
#include "SiPMSensl.h"
#include "Next100SiPM.h"
#include "OpticalMaterialProperties.h"
#include "BoxPointSampler.h"
#include "Visibilities.h"

#include <G4GenericMessenger.hh>
#include <G4Box.hh>
#include <G4Tubs.hh>
#include <G4LogicalVolume.hh>
#include <G4PVPlacement.hh>
#include <G4RotationMatrix.hh>
#include <G4Material.hh>
#include <G4NistManager.hh>
#include <G4OpticalSurface.hh>
#include <G4LogicalSkinSurface.hh>
#include <G4LogicalBorderSurface.hh>

using namespace nexus;


NextDemoSiPMBoard::NextDemoSiPMBoard():
  GeometryBase     (),
  verbosity_       (false),
  sipm_verbosity_  (false),
  visibility_      (false),
  sipm_visibility_ (true),
  sipm_coating_    (false),
  time_binning_    (1. * microsecond),
  num_columns_     (8),
  num_rows_        (8),
  num_sipms_       (num_rows_ * num_columns_),
  sipm_pitch_      (10.  * mm),
  side_reduction_  (0.5  * mm),
  kapton_thickn_   (0.3  * mm),
  mask_thickn_     (2.0  * mm),
  membrane_thickn_ (0.),
  coating_thickn_  (0.),
  hole_type_       (""),
  hole_diam_       (3.5  * mm),
  hole_x_          (0.0  * mm),
  hole_y_          (0.0  * mm),
  sipm_type_       (""),
  mother_phys_     (nullptr),
  kapton_gen_      (nullptr)
{
  msg_ = new G4GenericMessenger(this, "/Geometry/NextDemo/",
                                "Control commands of the NextDemo geometry.");

  msg_->DeclareProperty("sipm_board_verbosity",       verbosity_, "NextDemoSiPMBoard verbosity");
  msg_->DeclareProperty("sipm_verbosity"      ,  sipm_verbosity_, "NextDemoSiPMBoard SiPMs verbosity");
  msg_->DeclareProperty("sipm_board_vis"      ,      visibility_, "NextDemoSiPMBoard visibility.");
  msg_->DeclareProperty("sipm_visibility"     , sipm_visibility_, "NextDemoSiPMBoard SiPMs visibility");
  msg_->DeclareProperty("sipm_coating"        ,    sipm_coating_, "NextDemoSiPMBoard SiPMs coating");

  G4GenericMessenger::Command& time_binning_cmd = msg_->DeclareProperty("sipm_time_binning", time_binning_, "TP SiPMs time binning.");
  time_binning_cmd.SetParameterName("sipm_time_binning", false);
  time_binning_cmd.SetUnitCategory("Time");
  time_binning_cmd.SetRange("sipm_time_binning>0.");
}


NextDemoSiPMBoard::~NextDemoSiPMBoard()
{
  delete msg_;
  delete kapton_gen_;
  delete sipm_;
}


void NextDemoSiPMBoard::Construct()
{

  G4double sipm_z_dim = 0.;
  G4RotationMatrix* sipm_rot = new G4RotationMatrix();

  if (sipm_type_ == "sensl"){
    SiPMSensl* sipm = new SiPMSensl();

    sipm->SetVisibility(sipm_visibility_);
    sipm->SetTimeBinning(time_binning_);
    sipm->SetSensorDepth(3);
    sipm->SetMotherDepth(5);
    sipm->SetNamingOrder(1000);
    sipm->Construct();

    sipm_ = sipm;

    sipm_z_dim = sipm->GetDimensions().z();
    sipm_rot->rotateY(pi);
  }
  else if (sipm_type_ == "next100"){
    Next100SiPM* sipm = new Next100SiPM();

    sipm->SetVisibility(sipm_visibility_);
    sipm->SetSiPMCoating(sipm_coating_);
    sipm->SetTimeBinning(time_binning_);
    sipm->SetSensorDepth(2);
    sipm->SetMotherDepth(4);
    sipm->SetNamingOrder(1000);
    sipm->Construct();

    sipm_ = sipm;

    sipm_z_dim = sipm->GetDimensions().z();
    sipm_rot->rotateY(0.0);
  }

  /// Make sure the mother physical volume is actually valid
  if (!mother_phys_)
    G4Exception("[NextDemoSiPMBoard]", "Construct()",
                FatalException, "Mother physical volume is a nullptr.");

  G4Material* mother_gas = mother_phys_->GetLogicalVolume()->GetMaterial();


  /// Board configuration checks
  // Coating require membranes
  if (coating_thickn_ > 0.)
    if (membrane_thickn_ == 0.)
      G4Exception("[NextDemoSiPMBoard]", "Construct()", FatalException,
      "Coating require membranes");

  // Membranes require masks
  if (membrane_thickn_ > 0.)
    if (mask_thickn_ == 0.)
      G4Exception("[NextDemoSiPMBoard]", "Construct()", FatalException,
      "Membranes require masks");

  // Masks require holes
  if (mask_thickn_ > 0.)
    if (hole_diam_ == 0.)
      G4Exception("[NextDemoSiPMBoard]", "Construct()", FatalException,
      "Masks require holes");


  /// Board-Wrapper volume that contains all other elements
  G4String board_name = "SIPM_BOARD";

  // Calculating board wrapper dimensions
  G4double board_size_x = num_columns_ * sipm_pitch_ - 2. * side_reduction_;
  G4double board_size_y = num_rows_    * sipm_pitch_ - 2. * side_reduction_;
  G4double board_size_z = kapton_thickn_ + coating_thickn_ +
                          std::max(sipm_z_dim, mask_thickn_);

  board_size_ = G4ThreeVector(board_size_x, board_size_y, board_size_z);

  G4Box* board_solid = new G4Box(board_name, board_size_x/2.,
                                 board_size_y/2., board_size_z/2.);

  G4LogicalVolume* board_logic =
    new G4LogicalVolume(board_solid, mother_gas, board_name);

  GeometryBase::SetLogicalVolume(board_logic);


  /// Kapton
  G4String kapton_name = "KAPTON_BOARD";

  G4double kapton_posz = - board_size_z/2. + kapton_thickn_/2.;

  G4Box* kapton_solid = new G4Box(kapton_name, board_size_x/2.,
                                  board_size_y/2., kapton_thickn_/2.);

  G4LogicalVolume* kapton_logic =
    new G4LogicalVolume(kapton_solid,
                        G4NistManager::Instance()->FindOrBuildMaterial("G4_KAPTON"),
                        kapton_name);

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., kapton_posz), kapton_logic,
                    kapton_name, board_logic, false, 0, true);


  /// Teflon Masks
  G4String mask_name = "BOARD_MASK";

  G4double mask_posz = - board_size_z/2. + kapton_thickn_ + mask_thickn_/2.;

  G4Box* mask_solid = new G4Box(mask_name, board_size_x/2.,
                                board_size_y/2., mask_thickn_/2.);

  G4LogicalVolume* mask_logic =
    new G4LogicalVolume(mask_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_TEFLON"),
                        mask_name);

  // Adding the optical surface
  G4OpticalSurface* mask_opsurf =
    new G4OpticalSurface(mask_name, unified, ground, dielectric_metal);
  mask_opsurf->SetMaterialPropertiesTable(OpticalMaterialProperties::PTFE());
  new G4LogicalSkinSurface(mask_name + "_OPSURF", mask_logic, mask_opsurf);

  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., mask_posz), mask_logic,
                    mask_name, board_logic, false, 0, true);


  /// Mask Holes
  G4VSolid* hole_solid = nullptr;
  G4String hole_name = "BOARD_MASK_HOLE";
  if (hole_type_ == "rounded"){
    hole_solid = new G4Tubs(hole_name, 0., hole_diam_/2., mask_thickn_/2., 0, 360.*deg);}
  else if (hole_type_ == "rectangular"){
    hole_solid = new G4Box(hole_name, hole_x_/2., hole_y_/2., mask_thickn_/2.);}

  G4LogicalVolume* hole_logic =
    new G4LogicalVolume(hole_solid, mother_gas, hole_name);

  // Generate SiPM positions
  GenerateSiPMPositions();

  G4double sipm_posz = - mask_thickn_/2. + sipm_z_dim/2.;

  new G4PVPlacement(sipm_rot, G4ThreeVector(0., 0., sipm_posz), sipm_->GetLogicalVolume(),
                    sipm_->GetLogicalVolume()->GetName(), hole_logic,
                    false, 0, false);


  /// Membranes
  G4LogicalVolume* membrane_logic;

  if (membrane_thickn_ > 0.) {
    G4String membrane_name = "BOARD_MASK_MEMB";

    G4double membrane_posz = mask_thickn_/2. - membrane_thickn_/2.;

    G4Tubs* membrane_solid = new G4Tubs(membrane_name, 0., hole_diam_/2.,
                                        membrane_thickn_/2., 0, 360.*deg);

    membrane_logic =
      //new G4LogicalVolume(membrane_solid, G4NistManager::Instance()->FindOrBuildMaterial("G4_TEFLON"),
      //                    membrane_name);
      new G4LogicalVolume(membrane_solid, mother_gas, membrane_name);

    new G4PVPlacement(nullptr, G4ThreeVector(0., 0., membrane_posz), membrane_logic,
                      membrane_name, hole_logic, false, 0, true);
  }


  /// Placing the Holes with SiPMs & membranes inside
  for (G4int sipm_id=0; sipm_id<num_sipms_; sipm_id++) {
    new G4PVPlacement(nullptr, sipm_positions_[sipm_id], hole_logic,
                      hole_name, mask_logic, false, sipm_id, false);
  }


  /// COATING
  G4LogicalVolume* coating_logic;

  if (coating_thickn_ > 0.) {
    G4String coating_name = "BOARD_COATING";

    G4double coating_posz = board_size_z/2. - coating_thickn_/2.;

    G4Box* coating_solid = new G4Box(coating_name, board_size_x/2.,
                                     board_size_y/2., coating_thickn_/2.);

    G4Material* tpb = MaterialsList::TPB();
    tpb->SetMaterialPropertiesTable(OpticalMaterialProperties::TPB());

    coating_logic = new G4LogicalVolume(coating_solid, tpb, coating_name);

    G4PVPlacement* coating_phys =
      new G4PVPlacement(nullptr, G4ThreeVector(0.,0.,coating_posz), coating_logic,
                        coating_name, board_logic, false, 0, true);

    // Optical surface
    G4OpticalSurface* coating_opsurf =
      new G4OpticalSurface(coating_name + "_OPSURF", glisur, ground,
                           dielectric_dielectric, .01);

    new G4LogicalBorderSurface("TEFLON_WLS_GAS_OPSURF", coating_phys,
                               mother_phys_, coating_opsurf);
    new G4LogicalBorderSurface("GAS_TEFLON_WLS_OPSURF", mother_phys_,
                               coating_phys, coating_opsurf);

  }


  /// VERTEX GENERATOR
  kapton_gen_ = new BoxPointSampler(board_size_x, board_size_y, kapton_thickn_, 0.,
                                    G4ThreeVector(0., 0., -board_size_z/2. + kapton_thickn_/2.),
                                    nullptr);


  /// VERBOSITY
  if (verbosity_)
    G4cout << "* SiPM board size:    " << board_size_      << G4endl;
    G4cout << "* " << num_sipms_ << " SiPMs from Sensl"    << G4endl;

    if (sipm_verbosity_) {
      for (G4int sipm_num=0; sipm_num<num_sipms_; sipm_num++)
        G4cout << "* SiPM " << sipm_num << " position: " << sipm_positions_[sipm_num] << G4endl;
    }

    G4cout << "* Kapton thickness:   " << kapton_thickn_   << G4endl;
    G4cout << "* Mask thickness:     " << mask_thickn_     << G4endl;
    G4cout << "* Mask hole diameter: " << hole_diam_       << G4endl;
    G4cout << "* Membrane thickness: " << membrane_thickn_ << G4endl;
    G4cout << "* Coating thickness: "  << coating_thickn_  << G4endl;


  /// VISIBILITIES
  if (visibility_) {
    board_logic  ->SetVisAttributes(G4VisAttributes::Invisible);
    kapton_logic ->SetVisAttributes(Blue());
    mask_logic   ->SetVisAttributes(LightBlue());
    hole_logic   ->SetVisAttributes(G4VisAttributes::Invisible);
  }
  else{
    board_logic  ->SetVisAttributes(G4VisAttributes::Invisible);
    kapton_logic ->SetVisAttributes(G4VisAttributes::Invisible);
    mask_logic   ->SetVisAttributes(G4VisAttributes::Invisible);
    hole_logic   ->SetVisAttributes(G4VisAttributes::Invisible);
  }
  if (membrane_thickn_ > 0.) membrane_logic->SetVisAttributes(G4VisAttributes::Invisible);
  if (coating_thickn_  > 0.) coating_logic ->SetVisAttributes(G4VisAttributes::Invisible);
}



G4ThreeVector NextDemoSiPMBoard::GenerateVertex(const G4String&) const
{
  // Only one generation region available at the moment
  return kapton_gen_->GenerateVertex("INSIDE");
}



void NextDemoSiPMBoard::GenerateSiPMPositions()
{
  G4double margin    = sipm_pitch_/2. - side_reduction_;

  for (auto i=0; i<8; i++) {
    G4double sipm_posy = -board_size_.y()/2. + margin + i * sipm_pitch_;

    for (auto j=0; j<8; j++) {
      G4double sipm_posx = +board_size_.x()/2. - margin - j * sipm_pitch_;

      sipm_positions_.push_back(G4ThreeVector(sipm_posx, sipm_posy, 0.));
    }
  }
}

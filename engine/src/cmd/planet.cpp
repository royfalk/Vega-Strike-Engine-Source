#include <math.h>
#include "vegastrike.h"
#include "unit_factory.h"
#include "planet.h"
#include "gfxlib.h"
#include "gfx/sphere.h"
#include "collection.h"
#include "ai/order.h"
#include "gfxlib_struct.h"
#include "vs_globals.h"
#include "config_xml.h"
#include <assert.h>
#include "cont_terrain.h"
#include "atmosphere.h"
#ifdef FIX_TERRAIN
#include "gfx/planetary_transform.h"
#endif

#include "collide2/CSopcodecollider.h"
#include "images.h"
#include "gfx/halo.h"
#include "gfx/animation.h"
#include "cmd/script/flightgroup.h"
#include "gfx/ring.h"
#include "alphacurve.h"
#include "gfx/vsimage.h"
#include "vsfilesystem.h"
#include "gfx/camera.h"
#include "universe_util.h"
#include "galaxy_xml.h"


extern float ScaleJumpRadius( float );
extern Flightgroup * getStaticBaseFlightgroup( int faction );
extern const vector< string >& ParseDestinations( const string &value );

char * getnoslash( char *inp )
{
    char *tmp = inp;
    for (unsigned int i = 0; inp[i] != '\0'; i++)
        if (inp[i] == '/' || inp[i] == '\\')
            tmp = inp+i+1;
    return tmp;
}

string getCargoUnitName( const char *textname )
{
    char *tmp2 = strdup( textname );
    char *tmp  = getnoslash( tmp2 );
    unsigned int i;
    for (i = 0; tmp[i] != '\0' && (isalpha( tmp[i] ) || tmp[i] == '_'); i++) {}
    if (tmp[i] != '\0')
        tmp[i] = '\0';
    string retval( tmp );
    free( tmp2 );
    return retval;
}

string GetElMeshName( string name, string faction, char direction )
{
    using namespace VSFileSystem;
    char    strdir[2]     = {direction, 0};
    string  elxmesh       = string( strdir )+"_elevator.bfxm";
    string  elevator_mesh = name+"_"+faction+elxmesh;
    VSFile  f;
    VSError err = f.OpenReadOnly( elevator_mesh, MeshFile );
    if (err > Ok)
        f.Close();
    else elevator_mesh = name+elxmesh;
    return elevator_mesh;
}


/* Constructors */
/*GamePlanet::GamePlanet() :
    GameUnit< DummyUnit > ( 0 ),
    radius( 0.0f ),
    satellites()
{
    terrain      = NULL;
    radius       = 0.0;
    shine = NULL;
    inside       = false;
    Init();
    terraintrans = NULL;
    SetAI( new Order() );     //no behavior
    memset( &(this->shield), 0, sizeof (Unit::shield) );
    this->shield.number = 2;
}*/


GamePlanet::GamePlanet( QVector x,
                        QVector y,
                        float vely,
                        const Vector &rotvel,
                        float pos,
                        float gravity,
                        float radius,
                        const string &filename,
                        const string &technique,
                        const string &unitname,
                        BLENDFUNC blendSrc,
                        BLENDFUNC blendDst,
                        const vector< string > &dest,
                        const QVector &orbitcent,
                        Unit *parent,
                        const GFXMaterial &ourmat,
                        const vector< GFXLightLocal > &light,
                        int faction,
                        string fullname,
                        bool inside_out,
                        unsigned int lights_num) :
    GameUnit< DummyUnit > ( 0 )
{
  // Initialize some constants
  static float densityOfRock = XMLSupport::parse_float( vs_config->getVariable( "physics", "density_of_rock", "3" ) );
  static float densityOfJumpPoint =
      XMLSupport::parse_float( vs_config->getVariable( "physics", "density_of_jump_point", "100000" ) );
  static bool smartplanets = XMLSupport::parse_bool( vs_config->getVariable( "physics", "planets_can_have_subunits", "false" ) );
  static bool neutralplanets =
      XMLSupport::parse_bool( vs_config->getVariable( "physics", "planets_always_neutral", "true" ) );
  static float bodyradius = XMLSupport::parse_float( vs_config->getVariable( "graphics", "star_body_radius", ".33" ) );

  // The docking port is 20% bigger than the planet
  static float planetdockportsize    = XMLSupport::parse_float( vs_config->getVariable( "physics", "planet_port_size", "1.2" ) );
  static float planetdockportminsize =
      XMLSupport::parse_float( vs_config->getVariable( "physics", "planet_port_min_size", "300" ) );

  // Not sure how a planet can be a jump point. TODO: simplify this if others agree
  bool notJumppoint = dest.empty();

  // Add destinations - I assume this is a list of destinations you can auto-pilot from here.
  // Not sure why this isn't in system or what this really is.
  // TODO: investigate in another refactor
  for (unsigned int i = 0; i < dest.size(); ++i)
      AddDestination( dest[i] );

  // Call Unit::Init.
  Init();

  // Initialize class fields
  killed = false;
  inside = false;
  terraintrans = nullptr;
  atmospheric  = false;
  atmosphere = nullptr;
  terrain    = nullptr;
  terraintrans = nullptr;
  colTrees     = nullptr;
  this->radius   = radius;
  this->gravity  = gravity;
  this->fullname = fullname;
  hull = (4./3)*M_PI*radius*radius*radius*(notJumppoint ? densityOfRock : densityOfJumpPoint);
  this->Mass   = (4./3)*M_PI*radius*radius*radius*( notJumppoint ? densityOfRock : (densityOfJumpPoint/100000) );
  shine = nullptr;

  // Planet Size
  curr_physical_state.position = prev_physical_state.position = cumulative_transformation.position = orbitcent+x;

  corner_min.i = corner_min.j = corner_min.k = -this->radius;
  corner_max.i = corner_max.j = corner_max.k = this->radius;
  this->radial_size = this->radius;

  // Names
  // Some code to generate name from unit type if we don't get it directly
  string tempname = unitname.empty() ? ::getCargoUnitName( filename.c_str() ) : unitname;
  setFullname( tempname );
  name = fullname; // Initializes StringPool:Reference in unit_generic for some reason.

  // Set faction and behavior of planet
  // Can planets actually do something?
  SetAI( new PlanetaryOrbit( this, vely, pos, x, y, orbitcent, parent ) );
  this->faction = neutralplanets ? FactionUtil::GetNeutralFaction() : faction;

  // Set rotation of planet
  SetAngularVelocity( rotvel );

  // Docks
  if ( (!atmospheric) && notJumppoint ) {
      float dock = radius*planetdockportsize;
      if (dock-radius < planetdockportminsize)
          dock = radius+planetdockportminsize;
      pImage->dockingports.push_back( DockingPorts( Vector( 0, 0, 0 ), dock, 0, DockingPorts::Type::CONNECTED_OUTSIDE ) );
  }

  // Lights
  if (lights_num)
      radius *= bodyradius;

  for (unsigned int i = 0; i < lights_num; i++) {
      int l;
      GFXCreateLight( l, light[i].ligh, !light[i].islocal );
      this->lights.push_back( l );
    }

  std::cout << "Created planet " << fullname << " with " << lights_num << " (" <<
          this->lights.size() <<")\n";

  // Planet Glow?
  if (light.size() > 0) {
      static float bodyradius = XMLSupport::parse_float( vs_config->getVariable( "graphics", "star_body_radius", ".33" ) );
      static bool  drawglow   = XMLSupport::parse_bool( vs_config->getVariable( "graphics", "draw_star_glow", "true" ) );

      static bool  drawstar   = XMLSupport::parse_bool( vs_config->getVariable( "graphics", "draw_star_body", "true" ) );
      static float glowradius =
          XMLSupport::parse_float( vs_config->getVariable( "graphics", "star_glow_radius", "1.33" ) )/bodyradius;

      if (drawglow) {
          GFXColor    c = getMaterialEmissive( ourmat );
          static bool spec = XMLSupport::parse_bool( vs_config->getVariable( "graphics", "glow_ambient_star_light", "false" ) );
          static bool diff = XMLSupport::parse_bool( vs_config->getVariable( "graphics", "glow_diffuse_star_light", "false" ) );
          if (diff)
              c = light[0].ligh.GetProperties( DIFFUSE );
          if (spec)
              c = light[0].ligh.GetProperties( AMBIENT );
          static vector< string >shines = ParseDestinations( vs_config->getVariable( "graphics", "star_shine", "shine.ani" ) );
          if ( shines.empty() )
              shines.push_back( "shine.ani" );
          shine = new Animation( shines[rand()%shines.size()].c_str(), true, .1, BILINEAR, false, true, c );             //GFXColor(ourmat.er,ourmat.eg,ourmat.eb,ourmat.ea));
          shine->SetDimensions( glowradius*radius, glowradius*radius );
          if (!drawstar) {
              delete meshdata[0];
              meshdata.clear();
              meshdata.push_back( NULL );
          }
      }
  }

  // Wormhole code - jump point?
  bool wormhole = dest.size() != 0;
  if (wormhole) {
      static std::string wormhole_unit = vs_config->getVariable( "graphics", "wormhole", "wormhole" );
      string stab( ".stable" );
      if (rand() > RAND_MAX*.99)
        stab = ".unstable";
      string wormholename   = wormhole_unit+stab;
      string wormholeneutralname = wormhole_unit+".neutral"+stab;
      Unit  *jum = UnitFactory::createUnit( wormholename.c_str(), true, faction );
      int    neutralfaction = FactionUtil::GetNeutralFaction();
      faction = neutralfaction;

      Unit  *neujum  = UnitFactory::createUnit( wormholeneutralname.c_str(), true, neutralfaction );
      Unit  *jump    = jum;
      bool   anytrue = false;
      while (jump != NULL) {
          if (jump->name != "LOAD_FAILED") {
              anytrue = true;
              radius  = jump->rSize();
              Mesh *shield = jump->meshdata.size() ? jump->meshdata.back() : NULL;
              if ( jump->meshdata.size() ) jump->meshdata.pop_back();
              while ( jump->meshdata.size() ) {
                  this->meshdata.push_back( jump->meshdata.back() );
                  jump->meshdata.pop_back();
                }
              jump->meshdata.push_back( shield );
              for (un_iter i = jump->getSubUnits(); !i.isDone(); ++i)
                SubUnits.prepend( *i );
              jump->SubUnits.clear();
            }
          jump->Kill();
          if (jump != neujum)
            jump = neujum;
          else
            jump = NULL;
        }
      if (anytrue)
        meshdata.push_back( NULL );              //shield mesh...otherwise is a standard planet
      wormhole = anytrue;
    }

  if(!wormhole) {
      static int stacks = XMLSupport::parse_int( vs_config->getVariable( "graphics", "planet_detail", "24" ) );
      atmospheric = !(blendSrc == ONE && blendDst == ZERO);
      meshdata.push_back( new SphereMesh( radius, stacks, stacks, filename.c_str(), technique, NULL, inside_out, blendSrc, blendDst ) );
      meshdata.back()->setEnvMap( GFXFALSE );
      meshdata.back()->SetMaterial( ourmat );
      meshdata.push_back( NULL );
    }

  // No idea what this code does
  calculate_extent( false );
  if (wormhole) {
      static float radscale = XMLSupport::parse_float( vs_config->getVariable( "physics", "jump_mesh_radius_scale", ".5" ) );
      radius *= radscale;
      corner_min.i = corner_min.j = corner_min.k = -radius;
      corner_max.i = corner_max.j = corner_max.k = radius;
      radial_size  = radius;
      if ( !meshdata.empty() )
        meshdata[0]->setVirtualBoundingBox( corner_min, corner_max, radius );
    }


  // Things I should have disabled for now but didn't
  // Anything to do with cargo code, defense grids and smart planets

  int    tmpfac   = faction;
  if (UniverseUtil::LookupUnitStat( tempname, FactionUtil::GetFactionName( faction ), "Cargo_Import" ).length() == 0)
    tmpfac = FactionUtil::GetPlanetFaction();
  Unit  *un = UnitFactory::createUnit( tempname.c_str(), true, tmpfac );


  if ( un->name != string( "LOAD_FAILED" ) ) {
      pImage->cargo = un->GetImageInformation().cargo;
      pImage->CargoVolume   = un->GetImageInformation().CargoVolume;
      pImage->UpgradeVolume = un->GetImageInformation().UpgradeVolume;
      VSSprite *tmp = pImage->pHudImage;
      pImage->pHudImage     = un->GetImageInformation().pHudImage;
      un->GetImageInformation().pHudImage = tmp;
      maxwarpenergy = un->WarpCapData();
      if (smartplanets) {
          SubUnits.prepend( un );
          un->SetRecursiveOwner( this );
          this->SetTurretAI();
          un->SetTurretAI();              //allows adding planetary defenses, also allows launching fighters from planets, interestingly
          un->name = "Defense_grid";
        }


    }
  if ( un->name == string( "LOAD_FAILED" ) || (!smartplanets) )
    un->Kill();


  // Also disabled - planetray shields
  //Force shields to 0
  /*
   *  this->shield.number=2;
   *  this->shield.recharge=0;
   *  this->shield.shield2fb.frontmax=0;
   *  this->shield.shield2fb.backmax=0;
   *  this->shield.shield2fb.front=0;
   *  this->shield.shield2fb.back=0;
   */

  // More shield code
  memset( &(this->shield), 0, sizeof (Unit::shield) );
  this->shield.number = 2;
  if ( meshdata.empty() ) meshdata.push_back( nullptr );

}

static void SetFogMaterialColor( Mesh *thus, const GFXColor &color, const GFXColor &dcolor )
{
    static float emm  = XMLSupport::parse_float( vs_config->getVariable( "graphics", "atmosphere_emmissive", "1" ) );
    static float diff = XMLSupport::parse_float( vs_config->getVariable( "graphics", "atmosphere_diffuse", "1" ) );
    GFXMaterial  m;
    setMaterialAmbient( m, 0.0);
    setMaterialDiffuse( m, diff*dcolor);
    setMaterialSpecular( m, 0.0);
    setMaterialEmissive(m, emm*color);
    m.power = 0;
    thus->SetMaterial( m );
}
Mesh * MakeFogMesh( const AtmosphericFogMesh &f, float radius )
{
    static int count = 0;
    count++;
    string     nam   = f.meshname+XMLSupport::tostring( count )+".png";
    if (f.min_alpha != 0 || f.max_alpha != 255 || f.concavity != 0 || f.focus != .5 || f.tail_mode_start != -1
        || f.tail_mode_end != -1) {
        static int     rez = XMLSupport::parse_int( vs_config->getVariable( "graphics", "atmosphere_texture_resolution", "512" ) );
        unsigned char *tex = (unsigned char*) malloc( sizeof (char)*rez*4 );
        for (int i = 0; i < rez; ++i) {
            tex[i*4]   = 255;
            tex[i*4+1] = 255;
            tex[i*4+2] = 255;
            tex[i*4+3] = get_alpha( i, rez, f.min_alpha, f.max_alpha, f.focus, f.concavity, f.tail_mode_start, f.tail_mode_end );
        }
        //Writing in the homedir texture directory
        ::VSImage image;
        using VSFileSystem::TextureFile;
        image.WriteImage( (char*) nam.c_str(), &tex[0], PngImage, rez, 1, true, 8, TextureFile );
    }
    vector< string >override;
    override.push_back( nam );
    Mesh *ret = Mesh::LoadMesh( f.meshname.c_str(), Vector( f.scale*radius, f.scale*radius, f.scale*radius ), 0, NULL, override );
    ret->setConvex( true );
    SetFogMaterialColor( ret, GFXColor( f.er, f.eg, f.eb, f.ea ), GFXColor( f.dr, f.dg, f.db, f.da ) );
    return ret;
}

class AtmosphereHalo : public GameUnit< Unit >
{
public:
    float planetRadius;
    AtmosphereHalo( float radiusOfPlanet, vector< Mesh* > &meshes, int faction ) :
        GameUnit< Unit > ( meshes, true, faction )
    {
        planetRadius = radiusOfPlanet;
    }
    virtual void Draw( const Transformation &quat = identity_transformation, const Matrix &m = identity_matrix )
    {
        QVector dirtocam   = _Universe->AccessCamera()->GetPosition()-m.p;
        Transformation qua = quat;
        Matrix  mat = m;
        float   distance   = dirtocam.Magnitude();

        float   MyDistanceRadiusFactor = planetRadius/distance;
        float HorizonHeight = sqrt( 1-MyDistanceRadiusFactor*MyDistanceRadiusFactor )*planetRadius;

        float zscale;
        float xyscale = zscale = HorizonHeight/planetRadius;
        zscale = 0;
        dirtocam.Normalize();
        mat.p += sqrt( planetRadius*planetRadius-HorizonHeight*HorizonHeight )*dirtocam;
        ScaleMatrix( mat, Vector( xyscale, xyscale, zscale ) );
        qua.position = mat.p;
        GameUnit< Unit >::Draw( qua, mat );
    }
};
void GamePlanet::AddFog( const std::vector< AtmosphericFogMesh > &v, bool opticalillusion )
{
    if ( meshdata.empty() ) meshdata.push_back( NULL );
#ifdef MESHONLY
    Mesh *shield = meshdata.back();
    meshdata.pop_back();
#endif
    std::vector< Mesh* >fogs;
    for (unsigned int i = 0; i < v.size(); ++i) {
        Mesh *fog = MakeFogMesh( v[i], rSize() );
        fogs.push_back( fog );
    }
    Unit *fawg;
    if (opticalillusion)
        fawg = new AtmosphereHalo( this->rSize(), fogs, 0 );
    else
        fawg = UnitFactory::createUnit( fogs, true, 0 );
    fawg->setFaceCamera();
    getSubUnits().preinsert( fawg );
    fawg->hull /= fawg->GetHullPercent();
#ifdef MESHONLY
    meshdata.push_back( shield );
#endif
}

Unit* GamePlanet::beginElement( QVector x,
                            QVector y,
                            float vely,
                            const Vector &rotvel,
                            float pos,
                            float gravity,
                            float radius,
                            const string &filename,
                            const string &technique,
                            const string &unitname,
                            BLENDFUNC blendSrc,
                            BLENDFUNC blendDst,
                            const vector< string > &dest,
                            int level,
                            const GFXMaterial &ourmat,
                            const vector< GFXLightLocal > &ligh,
                            bool isunit,
                            int faction,
                            string fullname,
                            bool inside_out )
{
    //this function is OBSOLETE
    Unit *un = nullptr;
    if (level > 2) {
        un_iter satiterator = satellites.createIterator();
        assert( *satiterator );
        if ( (*satiterator)->isUnit() == PLANETPTR ) {
            un = ( (GamePlanet*) (*satiterator) )->beginElement( x, y, vely, rotvel, pos,
                                                             gravity, radius,
                                                             filename, technique, unitname,
                                                             blendSrc, blendDst,
                                                             dest,
                                                             level-1,
                                                             ourmat, ligh,
                                                             isunit,
                                                             faction, fullname,
                                                             inside_out );
        } else {
            VSFileSystem::vs_fprintf( stderr, "Planets are unable to orbit around units" );
        }
    } else {
        if (isunit == true) {
            Unit *sat_unit  = NULL;
            Flightgroup *fg = getStaticBaseFlightgroup( faction );
            satellites.prepend( sat_unit = UnitFactory::createUnit( filename.c_str(), false, faction, "", fg, fg->nr_ships-1 ) );
            sat_unit->setFullname( fullname );
            un = sat_unit;
            un_iter satiterator( satellites.createIterator() );
            (*satiterator)->SetAI( new PlanetaryOrbit( *satiterator, vely, pos, x, y, QVector( 0, 0, 0 ), this ) );
            (*satiterator)->SetOwner( this );
        } else {
            if (dest.size() != 0)
                radius = ScaleJumpRadius( radius );
            DummyUnit *p = UnitFactory::createPlanet( x, y, vely, rotvel, pos, gravity, radius,
                                                       filename, technique, unitname,
                                                       blendSrc, blendDst, dest,
                                                       QVector( 0, 0, 0 ), this, ourmat, ligh, faction, fullname, inside_out );
            satellites.prepend( static_cast<GamePlanet*>(p) );
            un = p;
            p->SetOwner( this );
        }
    }
    return un;
}

void GamePlanet::AddCity( const std::string &texture,
                          float radius,
                          int numwrapx,
                          int numwrapy,
                          BLENDFUNC blendSrc,
                          BLENDFUNC blendDst,
                          bool inside_out,
                          bool reverse_normals )
{
    if ( meshdata.empty() )
        meshdata.push_back( NULL );
    Mesh *shield = meshdata.back();
    meshdata.pop_back();
    static float materialweight    = XMLSupport::parse_float( vs_config->getVariable( "graphics", "city_light_strength", "10" ) );
    static float daymaterialweight = XMLSupport::parse_float( vs_config->getVariable( "graphics", "day_city_light_strength", "0" ) );
    GFXMaterial  m;
    setMaterialAmbient( m, 0.0);
    setMaterialDiffuse( m, materialweight );
    setMaterialSpecular(m, 0.0);
    setMaterialEmissive(m, daymaterialweight );
    m.power = 0.0;
    static int stacks = XMLSupport::parse_int( vs_config->getVariable( "graphics", "planet_detail", "24" ) );
    meshdata.push_back( new CityLights( radius, stacks, stacks, texture.c_str(), numwrapx, numwrapy, inside_out, ONE, ONE,
                                        false, 0, M_PI, 0.0, 2*M_PI, reverse_normals ) );
    meshdata.back()->setEnvMap( GFXFALSE );
    meshdata.back()->SetMaterial( m );

    meshdata.push_back( shield );
}

Vector GamePlanet::AddSpaceElevator( const std::string &name, const std::string &faction, char direction )
{
    Vector dir, scale;
    switch (direction)
    {
    case 'u':
        dir.Set( 0, 1, 0 );
        break;
    case 'd':
        dir.Set( 0, -1, 0 );
        break;
    case 'l':
        dir.Set( -1, 0, 0 );
        break;
    case 'r':
        dir.Set( 1, 0, 0 );
        break;
    case 'b':
        dir.Set( 0, 0, -1 );
        break;
    default:
        dir.Set( 0, 0, 1 );
        break;
    }
    Matrix ElevatorLoc( Vector( dir.j, dir.k, dir.i ), dir, Vector( dir.k, dir.i, dir.j ) );
    scale = dir*radius+Vector( 1, 1, 1 )-dir;
    Mesh  *shield = meshdata.back();
    string elevator_mesh = GetElMeshName( name, faction, direction );     //filename
    Mesh  *tmp    = meshdata.back() = Mesh::LoadMesh( elevator_mesh.c_str(),
                                                      scale,
                                                      FactionUtil::
                                                      GetFactionIndex( faction ),
                                                      NULL );

    meshdata.push_back( shield );
    {
        //subunit computations
        Vector mn( tmp->corner_min() );
        Vector mx( tmp->corner_max() );
        if (dir.Dot( Vector( 1, 1, 1 ) ) > 0)
            ElevatorLoc.p.Set( dir.i*mx.i, dir.j*mx.j, dir.k*mx.k );
        else
            ElevatorLoc.p.Set( -dir.i*mn.i, -dir.j*mn.j, -dir.k*mn.k );
        Unit *un = UnitFactory::createUnit( name.c_str(), true, FactionUtil::GetFactionIndex( faction ), "", NULL );
        if (pImage->dockingports.back().GetPosition().MagnitudeSquared() < 10)
            pImage->dockingports.clear();
        pImage->dockingports.push_back( DockingPorts( ElevatorLoc.p, un->rSize()*1.5, 0, DockingPorts::Type::INSIDE ) );
        un->SetRecursiveOwner( this );
        un->SetOrientation( ElevatorLoc.getQ(), ElevatorLoc.getR() );
        un->SetPosition( ElevatorLoc.p );
        SubUnits.prepend( un );
    }
    return dir;
}

void GamePlanet::endElement() {}

GamePlanet* GamePlanet::GetTopPlanet( int level )
{
    if (level > 2) {
        un_iter satiterator = satellites.createIterator();
        assert( *satiterator );
        if ( (*satiterator)->isUnit() == PLANETPTR ) {
            return ( (GamePlanet*) (*satiterator) )->GetTopPlanet( level-1 );
        } else {
            VSFileSystem::vs_fprintf( stderr, "Planets are unable to orbit around units" );
            return NULL;
        }
    } else {
        return this;
    }
}

void GamePlanet::AddSatellite( Unit *orbiter )
{
    satellites.prepend( orbiter );
    orbiter->SetOwner( this );
}

void GamePlanet::AddAtmosphere( const std::string &texture,
                                float radius,
                                BLENDFUNC blendSrc,
                                BLENDFUNC blendDst,
                                bool inside_out )
{
    if ( meshdata.empty() )
        meshdata.push_back( NULL );
    Mesh *shield = meshdata.back();
    meshdata.pop_back();
    static int stacks = XMLSupport::parse_int( vs_config->getVariable( "graphics", "planet_detail", "24" ) );
    meshdata.push_back( new SphereMesh( radius, stacks, stacks, texture.c_str(), string(), NULL, inside_out, blendSrc, blendDst ) );
    if ( meshdata.back() ) {
        //By klauss - this needs to be done for most atmospheres
        GFXMaterial a = {
            0, 0, 0, 0,
            1, 1, 1, 1,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0
        };
        meshdata.back()->SetMaterial( a );
    }
    meshdata.push_back( shield );
}
void GamePlanet::AddRing( const std::string &texture,
                          float iradius,
                          float oradius,
                          const QVector &R,
                          const QVector &S,
                          int slices,
                          int wrapx,
                          int wrapy,
                          BLENDFUNC blendSrc,
                          BLENDFUNC blendDst )
{
    if ( meshdata.empty() )
        meshdata.push_back( NULL );
    Mesh *shield = meshdata.back();
    meshdata.pop_back();
    static int stacks = XMLSupport::parse_int( vs_config->getVariable( "graphics", "planet_detail", "24" ) );
    if (slices > 0) {
        stacks = stacks;
        if (stacks < 3)
            stacks = 3;
        for (int i = 0; i < slices; i++)
            meshdata.push_back( new RingMesh( iradius, oradius, stacks, texture.c_str(), R, S, wrapx, wrapy, blendSrc, blendDst,
                                             false, i*(2*M_PI)/( (float) slices ), (i+1)*(2*M_PI)/( (float) slices ) ) );
    }
    meshdata.push_back( shield );
}





string GamePlanet::getHumanReadablePlanetType() const
{
    //static std::map<std::string, std::string> planetTypes (readPlanetTypes("planet_types.xml"));
    //return planetTypes[getCargoUnitName()];
    return _Universe->getGalaxy()->getPlanetNameFromTexture( getCargoUnitName() );
}

vector< UnitContainer* >PlanetTerrainDrawQueue;
void GamePlanet::Draw( const Transformation &quat, const Matrix &m )
{
    //Do lighting fx
    //if cam inside don't draw?
    //if(!inside) {
    GameUnit< DummyUnit >::Draw( quat, m );
    //}
    QVector    t( _Universe->AccessCamera()->GetPosition()-Position() );
    static int counter = 0;
    if (counter++ > 100) {
        if (t.Magnitude() < corner_max.i) {
            inside = true;
        } else {
            inside = false;
            ///somehow warp unit to reasonable place outisde of planet
            if (terrain) {
#ifdef PLANETARYTRANSFORM
                terrain->DisableUpdate();
#endif
            }
        }
    }
    GFXLoadIdentity( MODEL );
    for (unsigned int i = 0; i < lights.size(); i++)
        GFXSetLight( lights[i], POSITION, GFXColor( cumulative_transformation.position.Cast() ) );
    if (inside && terrain)
        PlanetTerrainDrawQueue.push_back( new UnitContainer( this ) );
    if (shine) {
        Vector  p, q, r;
        QVector c;
        MatrixToVectors( cumulative_transformation_matrix, p, r, q, c );
        shine->SetOrientation( p, q, r );
        shine->SetPosition( c );
        static int num_shine_drawing =
            XMLSupport::parse_int( vs_config->getVariable( "graphics", "num_times_to_draw_shine", "2" ) );
        for (int i = 0; i < num_shine_drawing; ++i)
            shine->Draw();
    }
}
void GamePlanet::ProcessTerrains()
{
    while ( !PlanetTerrainDrawQueue.empty() ) {
        GamePlanet *pl = (GamePlanet*) PlanetTerrainDrawQueue.back()->GetUnit();
        pl->DrawTerrain();
        PlanetTerrainDrawQueue.back()->SetUnit( NULL );
        delete PlanetTerrainDrawQueue.back();
        PlanetTerrainDrawQueue.pop_back();
    }
}

void GamePlanet::DrawTerrain()
{
    inside = true;
    if (terrain)
        terrain->EnableUpdate();
#ifdef PLANETARYTRANSFORM
    TerrainUp = t;
    Normalize( TerrainUp );
    TerrainH  = TerrainUp.Cross( Vector( -TerrainUp.i+.25, TerrainUp.j-.24, -TerrainUp.k+.24 ) );
    Normalize( TerrainH );
#endif

    GFXLoadIdentity( MODEL );
    if (inside && terrain) {
        _Universe->AccessCamera()->UpdatePlanetGFX();
        terrain->SetTransformation( *_Universe->AccessCamera()->GetPlanetGFX() );
        terrain->AdjustTerrain( _Universe->activeStarSystem() );
        terrain->Draw();
#ifdef PLANETARYTRANSFORM
        terraintrans->GrabPerpendicularOrigin( _Universe->AccessCamera()->GetPosition(), tmp );
        terrain->SetTransformation( tmp );
        terrain->AdjustTerrain( _Universe->activeStarSystem() );
        terrain->Draw();
        if (atmosphere) {
            Vector tup( tmp[4], tmp[5], tmp[6] );
            Vector p    = ( _Universe->AccessCamera()->GetPosition() );
            Vector blah = p-Vector( tmp[12], tmp[13], tmp[14] );
            blah    = p-( blah.Dot( tup ) )*tup;
            tmp[12] = blah.i;
            tmp[13] = blah.j;
            tmp[14] = blah.k;
            atmosphere->SetMatricesAndDraw( _Universe->AccessCamera()->GetPosition(), tmp );
        }
#endif
    }
}

extern bool CrashForceDock( Unit *thus, Unit *dockingUn, bool force );
extern void abletodock( int dock );
void GamePlanet::reactToCollision( Unit *un,
                                   const QVector &biglocation,
                                   const Vector &bignormal,
                                   const QVector &smalllocation,
                                   const Vector &smallnormal,
                                   float dist )
{
#ifdef JUMP_DEBUG
    VSFileSystem::vs_fprintf( stderr, "%s reacting to collision with %s drive %d", name.c_str(),
                              un->name.c_str(), un->GetJumpStatus().drive );
#endif
#ifdef FIX_TERRAIN
    if (terrain && un->isUnit() != PLANETPTR) {
        un->SetPlanetOrbitData( terraintrans );
        Matrix top;
        Identity( top );
        /*
         *  Vector posRelToTerrain = terraintrans->InvTransform(un->LocalPosition());
         *  top[12]=un->Position().i- posRelToTerrain.i;
         *  top[13]=un->Position().j- posRelToTerrain.j;
         *  top[14]=un->Position().k- posRelToTerrain.k;
         */
        Vector P, Q, R;
        un->GetOrientation( P, Q, R );
        terraintrans->InvTransformBasis( top, P, Q, R, un->Position() );
        Matrix inv, t;

        InvertMatrix( inv, top );
        VectorAndPositionToMatrix( t, P, Q, R, un->Position() );
        MultMatrix( top, t, inv );
#ifdef PLANETARYTRANSFORM
        terraintrans->GrabPerpendicularOrigin( un->Position(), top );
        static int tmp = 0;
#endif
        terrain->Collide( un, top );
    }
#endif
    jumpReactToCollision( un );
    //screws with earth having an atmosphere... blahrgh
    if (!terrain && GetDestinations().empty() && !atmospheric) {
        //no place to go and acts like a ship
        GameUnit< DummyUnit >::reactToCollision( un, biglocation, bignormal, smalllocation, smallnormal, dist );
        static bool planet_crash_docks =
            XMLSupport::parse_bool( vs_config->getVariable( "physics", "planet_collision_docks", "true" ) );
        if (_Universe->isPlayerStarship( un ) && planet_crash_docks)
            CrashForceDock( this, un, true );
    }
    //nothing happens...you fail to do anythign :-)
    //maybe air reisstance here? or swithc dynamics to atmos mode
}
void GamePlanet::EnableLights()
{
    for (unsigned int i = 0; i < lights.size(); i++)
        GFXEnableLight( lights[i] );
}
void GamePlanet::DisableLights()
{
    for (unsigned int i = 0; i < lights.size(); i++)
        GFXDisableLight( lights[i] );
}
GamePlanet::~GamePlanet()
{
    if (shine)
        delete shine;
    if (terrain)
        delete terrain;
    if (atmosphere)
        delete atmosphere;
    if (terraintrans) {
        Matrix *tmp = new Matrix();
        *tmp = cumulative_transformation_matrix;
        //terraintrans->SetTransformation (tmp);
    }
#ifdef FIX_TERRAIN
    if (terraintrans) {
        Matrix *tmp = new Matrix();
        *tmp = cumulative_transformation_matrix;
        terraintrans->SetTransformation( tmp );
        //FIXME
        //We're losing memory here...but alas alas... planets don't die that often
    }
#endif
}

PlanetaryTransform* GamePlanet::setTerrain( ContinuousTerrain *t, float ratiox, int numwraps, float scaleatmos )
{
    terrain = t;
    terrain->DisableDraw();
    float x, z;
    t->GetTotalSize( x, z );
#ifdef FIX_TERRAIN
    terraintrans = new PlanetaryTransform( .8*corner_max.i, x*ratiox, z, numwraps, scaleatmos );
    terraintrans->SetTransformation( &cumulative_transformation_matrix );

    return terraintrans;
#endif
    return NULL;
}

void GamePlanet::setAtmosphere( Atmosphere *t )
{
    atmosphere = t;
}

void GamePlanet::Kill( bool erasefromsave )
{
    Unit *tmp;
    for (un_iter iter = satellites.createIterator(); (tmp=*iter)!=NULL; ++iter)
        tmp->SetAI( new Order );
    /* probably not FIXME...right now doesn't work on paged out systems... not a big deal */
    for (unsigned int i = 0; i < this->lights.size(); i++)
        GFXDeleteLight( lights[i] );
    /*	*/
    satellites.clear();
    insiders.clear();
    GameUnit< DummyUnit >::Kill( erasefromsave );
}

PlanetaryOrbit::PlanetaryOrbit( Unit *p,
                                double velocity,
                                double initpos,
                                const QVector &x_axis,
                                const QVector &y_axis,
                                const QVector &centre,
                                Unit *targetunit ) : Order( MOVEMENT, 0 )
    , velocity( velocity )
    , theta( initpos )
    , inittheta( initpos )
    , x_size( x_axis )
    , y_size( y_axis )
    , current_orbit_frame( 0 )
{
    for (unsigned int t = 0; t < NUM_ORBIT_AVERAGE; ++t)
        orbiting_average[t] = QVector( 0, 0, 0 );
    orbiting_last_simatom = SIMULATION_ATOM;
    orbit_list_filled     = false;
    p->SetResolveForces( false );
    double delta = x_size.Magnitude()-y_size.Magnitude();
    if (delta == 0)
        focus = QVector( 0, 0, 0 );
    else if (delta > 0)
        focus = x_size*( delta/x_size.Magnitude() );
    else
        focus = y_size*( -delta/y_size.Magnitude() );
    if (targetunit) {
        type    = (MOVEMENT);
        subtype = (SSELF);
        AttachSelfOrder( targetunit );
    } else {
        type    = (MOVEMENT);
        subtype = (SLOCATION);
        AttachOrder( centre );
    }
    const double div2pi = ( 1.0/(2.0*PI) );

    this->SetParent( p );
}

PlanetaryOrbit::~PlanetaryOrbit()
{
    parent->SetResolveForces( true );
}

void PlanetaryOrbit::Execute()
{
    bool mining = parent->rSize() > 1444 && parent->rSize() < 1445;
    bool done   = this->done;
    this->Order::Execute();
    this->done = done;     //we ain't done till the cows come home
    if (done)
        return;
    QVector origin( targetlocation );
    static float orbit_centroid_averaging = XMLSupport::parse_float( vs_config->getVariable( "physics", "orbit_averaging", "16" ) );
    float   averaging = (float) orbit_centroid_averaging/(float) (parent->predicted_priority+1.0f);
    if (averaging < 1.0f) averaging = 1.0f;
    if (subtype&SSELF) {
        Unit *unit = group.GetUnit();
        if (unit) {
            unsigned int o = current_orbit_frame++;
            current_orbit_frame %= NUM_ORBIT_AVERAGE;
            if (current_orbit_frame == 0)
                orbit_list_filled = true;
            QVector desired = unit->prev_physical_state.position;
            if (orbiting_average[o].i == 0 && orbiting_average[o].j == 0 && orbiting_average[o].k == 0) {
                //clear all of them.
                for (o = 0; o < NUM_ORBIT_AVERAGE; o++)
                    orbiting_average[o] = desired;
                orbiting_last_simatom = SIMULATION_ATOM;
                current_orbit_frame   = 2;
                orbit_list_filled     = false;
            } else {
                if (SIMULATION_ATOM != orbiting_last_simatom) {
                    QVector sum_diff( 0, 0, 0 );
                    QVector sum_position;
                    int     limit;
                    if (orbit_list_filled) {
                        sum_position = orbiting_average[o];
                        limit = NUM_ORBIT_AVERAGE-1;
                        o = (o+1)%NUM_ORBIT_AVERAGE;
                    } else {
                        sum_position = orbiting_average[0];
                        limit = o;
                        o = 1;
                    }
                    for (int i = 0; i < limit; i++) {
                        sum_diff     += (orbiting_average[o]-orbiting_average[(o+NUM_ORBIT_AVERAGE-1)%NUM_ORBIT_AVERAGE]);
                        sum_position += orbiting_average[o];
                        o = (o+1)%NUM_ORBIT_AVERAGE;
                    }
                    if (limit != 0)
                        sum_diff *= ( 1./(limit) );
                    sum_position  *= ( 1./(limit+1) );

                    float ratio_simatom = (SIMULATION_ATOM/orbiting_last_simatom);
                    sum_diff      *= ratio_simatom;
                    unsigned int number_to_fill;
                    number_to_fill = (int) ( (NUM_ORBIT_AVERAGE/ratio_simatom)+.99 );
                    if (number_to_fill > NUM_ORBIT_AVERAGE) number_to_fill = NUM_ORBIT_AVERAGE;
                    if (ratio_simatom <= 1)
                        number_to_fill = NUM_ORBIT_AVERAGE;
                    //subtract it so the average remains the same.
                    sum_position += ( sum_diff*(number_to_fill/ -2.) );
                    for (o = 0; o < number_to_fill; o++) {
                        orbiting_average[o] = sum_position;
                        sum_position += sum_diff;
                    }
                    orbit_list_filled     = (o >= NUM_ORBIT_AVERAGE-1);
                    o %= NUM_ORBIT_AVERAGE;
                    current_orbit_frame   = (o+1)%NUM_ORBIT_AVERAGE;
                    orbiting_last_simatom = SIMULATION_ATOM;
                }
                orbiting_average[o] = desired;
            }
        } else {
            done = true;
            parent->SetResolveForces( true );
            return;             //flung off into space.
        }
    }
    QVector sum_orbiting_average( 0, 0, 0 );
    {
        int limit;
        if (orbit_list_filled)
            limit = NUM_ORBIT_AVERAGE;
        else
            limit = current_orbit_frame;
        for (int o = 0; o < limit; o++)
            sum_orbiting_average += orbiting_average[o];
        sum_orbiting_average *= 1./(limit == 0 ? 1 : limit);
    }
    const double div2pi = ( 1.0/(2.0*PI) );
    theta += velocity*SIMULATION_ATOM*div2pi;

    QVector x_offset    = cos( theta )*x_size;
    QVector y_offset    = sin( theta )*y_size;

    QVector destination = origin-focus+sum_orbiting_average+x_offset+y_offset;
    double  mag = ( destination-parent->LocalPosition() ).Magnitude();
    if (mining && 0) {
        printf( "(%.2f %.2f %.2f)\n(%.2f %.2f %.2f) del %.2f spd %.2f\n",
                parent->LocalPosition().i,
                parent->LocalPosition().j,
                parent->LocalPosition().k,
                destination.i,
                destination.j,
                destination.k,
                mag,
                mag*(1./SIMULATION_ATOM)
              );
    }
    parent->Velocity = parent->cumulative_velocity = ( ( ( destination-parent->LocalPosition() )*(1./SIMULATION_ATOM) ).Cast() );
    static float Unreasonable_value =
        XMLSupport::parse_float( vs_config->getVariable( "physics", "planet_ejection_stophack", "2000" ) );
    float v2 = parent->Velocity.Dot( parent->Velocity );
    if (v2 > Unreasonable_value*Unreasonable_value ) {
        parent->Velocity.Set( 0, 0, 0 );
        parent->cumulative_velocity.Set( 0, 0, 0 );
        parent->SetCurPosition( origin-focus+sum_orbiting_average+x_offset+y_offset );
    }
}

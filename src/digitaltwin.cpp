#include "digitaltwin.hpp"

#include <memory>
using namespace std;

#include "json.h"
#include <boost/asio.hpp>
#include <boost/process.hpp>
using namespace boost;
using json = nlohmann::json;

namespace digitaltwin {


struct Scene::Plugin 
{
    Plugin(): socket(io_context)
    {}

    asio::io_context io_context;
    asio::local::stream_protocol::socket socket;
    
    int viewport_size[2];
    vector<unsigned char> rgba_pixels;
    std::shared_ptr<process::child> backend;
};

Scene::Scene(int width,int height,string scene_path)
{
    md = new Plugin;
    md->rgba_pixels.resize(width * height * 4);
    md->viewport_size[1] = height,md->viewport_size[0] = width;
    load(scene_path);
}

Scene::~Scene()
{
    md->socket.close();
    delete md;
}

void Scene::load(string scene_path) {
    if(scene_path.empty()) return;

    try {
        cout << "Starting digital-twin service...";
        if(md->backend) {
            md->backend->terminate();
            md->backend->wait();
        }

        auto tmp_path = boost::filesystem::temp_directory_path().append("digitaltwin");
        md->backend = make_shared<process::child>("digitaltwin",scene_path,to_string(md->viewport_size[0]),to_string(md->viewport_size[1]),tmp_path);
        cout << "succeded" << endl;

        cout << "command:" << "digitaltwin" << ' '
             << scene_path << ' '
             << to_string(md->viewport_size[0]) << ' '
             << to_string(md->viewport_size[1]) << ' '
             << tmp_path << endl;
             
        auto socket_name = boost::filesystem::basename(scene_path) + ".json.sock";
        auto socket_path = tmp_path.append(socket_name);
        
        for(int i=0;i<4;i++) {
            cout << "Connecting to digital-twin...";
            md->backend->wait_for(chrono::seconds(1));
            
            system::error_code ec;           
            if (md->socket.connect(asio::local::stream_protocol::endpoint(socket_path.c_str()),ec)) {
                cout << "failed" << endl;
                cerr << ec.message() << endl;
            } else break;

            if (i==3) throw ec;
        }

        cout << "succeded" << endl;

        goto SUCCEDED;
    } catch(system::system_error& e) {
        cout << "failed" << endl;
        cerr << e.what() << endl;
    }

FAILED:
    md->socket.close();
    throw std::runtime_error("Failed to contact digtial-twin backend.");

SUCCEDED:
    ;
}

const Texture Scene::rtt() {
    stringstream req;
    req << "scene.rtt()" << endl;
    asio::write(md->socket,asio::buffer(req.str()));
    asio::read(md->socket,asio::buffer(md->rgba_pixels));
    
    Texture t;
    t.width = md->viewport_size[0];
    t.height = md->viewport_size[1];
    t.rgba_pixels = md->rgba_pixels.data();
    return t;
}

void Scene::play(bool run)
{
    stringstream req;
    req << "scene.play(" << (run ? "True" : "False") << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotate(double x,double y) 
{
    stringstream req;
    req << "scene.rotate(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotete_left() 
{

}

void Scene::rotete_right() 
{

}

void Scene::rotete_top()
{

}

void Scene::rotete_front()
{

}

void Scene::rotete_back() 
{

}

void Scene::pan(double x,double y) 
{
    stringstream req;
    req << "scene.pan(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::zoom(double factor) 
{
    stringstream req;
    req << "scene.zoom("<< factor <<")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

map<string,ActiveObject*> Scene::get_active_objs()
{
    return map<string,ActiveObject*>();
}

void Scene::set_logging(std::function<void(string)> log_callback)
{

}

Editor::Editor(Scene* sp) : scene(sp)
{

}

RayInfo Editor::ray(double x,double y) 
{
    stringstream req;
    req << "editor.ray(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));

    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;
    
    auto pos = json_res["pos"];
    return RayInfo {json_res["name"].get<string>(),{pos[0].get<double>(),pos[1].get<double>(),pos[2].get<double>()}};
}

void Editor::move(string name,Vec3 pos)
{
    stringstream req;
    req << "editor.move('" << name << "',[" << pos[0] << "," << pos[1] << "," << pos[2] << "])" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

ActiveObject* Editor::select(string name)
{
    stringstream req;
    req << "editor.select(" << name << ")" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));

    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;
    auto properties = json_res.dump();

    auto kind = json_res["kind"].get<string>();
    
    ActiveObject* obj = nullptr;
    
    if(kind == "Robot") {
        obj = new Robot(scene, properties);
    } else if(kind == "Camera3D") {
        obj = new Camera3D(scene, properties);
    } else if(kind == "Packer") {
        obj = new ActiveObject(scene, properties);
    }

    if(scene->active_objs_by_name.find(name) != scene->active_objs_by_name.end())
        *scene->active_objs_by_name[name] = *obj;
    else
        scene->active_objs_by_name[name] = obj;

    return scene->active_objs_by_name[name];
}

void Editor::add(string base,Vec3 pos,Vec3 rot,Vec3 scale) 
{

}

void Editor::remove(string name) 
{

}

void Editor::set_parent(string parent_name,string child_name)
{

}

void Editor::transparentize(string name,float value) 
{

}


void Editor::save() {
    stringstream req;
    req << "editor.save()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

ActiveObject::ActiveObject(Scene* sp, string properties) : scene(sp)
{
    auto json_properties = json::parse(properties);

    name = json_properties["name"].get<string>().c_str();
    kind = json_properties["kind"].get<string>().c_str();
    base = json_properties["base"].get<string>().c_str();
    auto pos = json_properties["pos"];
    auto rot = json_properties["rot"];
    copy(pos.begin(),pos.begin(),this->pos.begin());
    copy(rot.begin(),rot.begin(),this->rot.begin());
}

void ActiveObject::set_base(string path) 
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].set_base('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    cout << res.data().data() << endl;
    base = path;
}

Robot::Robot(Scene* sp, string properties) : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    end_effector = json_properties["end_effector"].get<string>();
}

void Robot::set_end_effector(string path)
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].set_end_effector('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    cout << res.data().data() << endl;
    end_effector = path;
    
    istream i(&res); i >> end_effector_id;
}

void Robot::digital_output(bool pickup) 
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].end_effector_obj.do(" << (pickup ? "True" : "False")  << ")"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

int Robot::get_joints_num()
{
    return 0;
}

void Robot::set_joint_position(int joint_index,float value)
{

}

float Robot::get_joint_position(int joint_index)
{

}

void Robot::set_end_effector_pos(Vec3 pos)
{

}

Vec3 Robot::get_end_effector_pos()
{

}

void Robot::set_end_effector_rot(Vec3 rot)
{

}

Vec3 Robot::get_end_effector_rot()
{

}

void Robot::set_home()
{

}

void Robot::home()
{

}

void Robot::set_speed(float value)
{

}

void Robot::track(bool enable)
{

}

Camera3D::Camera3D(Scene* sp,string properties)  : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    auto vs = json_properties["image_size"];
    image_size[0] = vs[0].get<long long>();
    image_size[1] = vs[1].get<long long>();
    rgba_pixels.resize(image_size[0]*image_size[1]*4);
    depth_pixels.resize(image_size[0]*image_size[1]*3);
    fov = json_properties["fov"].get<long long>();
    forcal = json_properties["forcal"].get<double>();
}

const Texture Camera3D::rtt() {
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].rtt()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::read(scene->md->socket,asio::buffer(rgba_pixels));
    asio::read(scene->md->socket,asio::buffer(depth_pixels));

    Texture t;
    t.width = image_size[0];
    t.height = image_size[1];
    t.rgba_pixels = rgba_pixels.data();
    t.depth_pixels = depth_pixels.data();

    return t;
};

void Camera3D::set_calibration(string params)
{

}


Placer::Placer(Scene* sp,string properties) : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    auto pos = json_properties["center"];
    copy(pos.begin(),pos.end(),center.begin());
    interval = json_properties["interval"].get<int>();
    amount = json_properties["amount"].get<int>();
    workpiece = json_properties["workpiece"].get<string>();
}

void Placer::set_workpiece(string base) 
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].workpiece = " << base << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_workpiece_texture(string img_path)
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].workpiece_texture = " << img_path << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_center(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].center = [" << pos[0] << ',' << pos[0] << ',' << pos[2] << ']' << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_amount(int num)
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].amount = " << num << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_interval(float seconds)
{
    stringstream req;
    req << "scene.active_objs_by_name["<<name<<"].interval = " << seconds << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}


void Placer::set_scale_factor(float max,float min) 
{

}

void Placer::set_place_mode(string place_mode)
{

}


Workflow::Workflow(Scene* sp) : scene(sp)   {}

string Workflow::get_active_obj_nodes() {
    stringstream req;
    req << "workflow.get_active_obj_nodes()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    return string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()));
}

void Workflow::set(string src) {
    stringstream req;
    req << "workflow.set('" << src << "')" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

string Workflow::get() {
    stringstream req;
    req << "workflow.get()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    return string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()));
}


void Workflow::start() {
    stringstream req;
    req << "workflow.start()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Workflow::stop() {
    stringstream req;
    req << "workflow.stop()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

}
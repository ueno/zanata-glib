const Zanata = imports.gi.Zanata;
const GLib = imports.gi.GLib;

let key_file = new GLib.KeyFile();
key_file.load_from_file(GLib.build_filenamev([GLib.get_user_config_dir(),
                                              'zanata.ini']),
                        GLib.KeyFileFlags.NONE);

let authorizer = new Zanata.KeyFileAuthorizer({ key_file: key_file });

let session = new Zanata.Session({ authorizer: authorizer,
                                   domain: 'translate_zanata_org' });

function checkProject(session, id) {
    session.get_project(id, null,
			function (s, res, d) {
			    let project = s.get_project_finish(res);
			    print(project.id == id);
			});
}

session.get_projects(null,
                     function (s, res, d) {
                         let result = s.get_projects_finish(res);
                         print(result.length);
                         for (let index in result) {
			     let project = result[index];
                             print([project.name, project.id, project.status]);
			     if (index == 0) {
				 checkProject(s, project.id);
			     }
                         }
                     });

let loop = GLib.MainLoop.new(null, false);
loop.run();

const Zanata = imports.gi.Zanata;
const GLib = imports.gi.GLib;
const Json = imports.gi.Json;

let key_file = new GLib.KeyFile();
key_file.load_from_file(GLib.build_filenamev([GLib.get_user_config_dir(),
                                              'zanata.ini']),
                        GLib.KeyFileFlags.NONE);

let authorizer = new Zanata.KeyFileAuthorizer({ key_file: key_file });

let session = new Zanata.Session({ authorizer: authorizer,
                                   domain: 'translate_zanata_org' });

function getTranslatedDocumentation(iteration) {
    iteration.get_translated_documentation(
        'coala', 'de-DE', null,
        function(s, res, d) {
            let stream = s.get_translated_documentation_finish(res);
            let parser = new Json.Parser();
            parser.load_from_stream(stream, null);
            let generator = new Json.Generator();
            generator.set_root(parser.get_root());
            generator.set_pretty(true);
            let [data,length] = generator.to_data();
            print(data);
        });
}

function getIterations(project) {
    project.get_iterations(null, function (s, res, d) {
        let result = s.get_iterations_finish(res);
        print(result.length);
        for (let index in result) {
            let iteration = result[index];
            print([iteration.id, iteration.status]);
        }
	if (result.length > 0) {
            getTranslatedDocumentation(result[result.length - 1]);
	}
    });
}

session.get_project('coala', null,
                    function (s, res, d) {
                        let project = s.get_project_finish(res);
                        getIterations(project);
                     });

let loop = GLib.MainLoop.new(null, false);
loop.run();

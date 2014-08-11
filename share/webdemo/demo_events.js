(function () {
  var daoVmSpace, daoStdio;
  var DaoInit, DaoQuit;
  var DaoVmSpace_Eval, DaoVmSpace_StdioStream, DaoStream_WriteChars;
  var demos = { "HelloWorld" : "io.writeln( \"Hello World!\" )" };
  var demo_dups = { "HelloWorld" : 0 };
  var printed = false;

  window.Module = {};
  window.Module['print'] = function (x) {
    terminal.setValue( terminal.getValue() + x + '\n' );
	printed = true;
  };

  jQuery(document).ready(function() {
    DaoInit = Module.cwrap("DaoInit", "number", ["string"]);
    DaoQuit = Module.cwrap("DaoQuit", "number");
    DaoVmSpace_Eval = Module.cwrap("DaoVmSpace_Eval", "number", ["number", "string"]);
	DaoVmSpace_StdioStream = Module.cwrap( "DaoVmSpace_StdioStream", "number", ["number"] );
	DaoStream_WriteChars = Module.cwrap( "DaoStream_WriteChars", "number", ["number", "string"] );

    daoVmSpace = DaoInit("");
	daoStdio = DaoVmSpace_StdioStream( daoVmSpace );

	var demo = "hello.dao";
    jQuery.get('/projects/Dao/doc/tip/demo/' + demo, function(data) {
		demos[ demo ] = data;
		demo_dups[ demo ] = 0;
		editor.setValue( data );
		editor.clearSelection();
		editor.session.setScrollTop(0);
    });

    jQuery("#submit-button").click(function() {
	
	  if( ! jQuery("#checkKeepHistory").is(':checked') ) terminal.setValue( '' );
	  printed = false;

      DaoVmSpace_Eval(daoVmSpace, editor.getValue());
	  if( printed == false ) DaoStream_WriteChars( daoStdio, "\n" );
    });


	jQuery('#select-demo').focus(function (){
		var demo = this.value;
		demos[ demo ] = editor.getValue();
	}).change(function() {
		var demo = jQuery(this).val(); 
		var codes = demos[ demo ];
		if( codes != undefined ){
			editor.setValue( codes );
			editor.clearSelection();
			editor.session.setScrollTop(0);
			return;
		}
        jQuery.get('/projects/Dao/doc/tip/demo/' + demo, function(data) {
			demos[ demo ] = data;
			demo_dups[ demo ] = 0;
			editor.setValue( data );
			editor.clearSelection();
			editor.session.setScrollTop(0);
        });
	});

    jQuery("#duplicate-demo").click(function() {
		var selected = jQuery('#select-demo').find(":selected");
		var item = selected.text();
		var demo = selected.val();
		var codes = editor.getValue();
		var dupid = demo_dups[ demo ] + 1;
		demos[ demo ] = codes;
		demo_dups[ demo ] += 1;
		demo += ' #' + dupid.toString();
		item += ' #' + dupid.toString();
		demos[ demo ] = codes;
		demo_dups[ demo ] = 0;
		jQuery("#select-demo").append('<option value="' + demo + '">' + item + '</option>');
		jQuery('#select-demo').val(demo);
	});


    window.onbeforeunload = function () {
		DaoQuit();
    }
  });
}());

/*********************************************************************
*								     *
* Initialization code.			                             *
*								     *
*********************************************************************/


$(function(){


    // A+ / A- buttons
    $('#aplus').click(function(ev) {
	ev.preventDefault();
	resize(1);
    });
    $('#aminus').click(function(ev) {
	ev.preventDefault();
	resize(0);
    });

    // Classic examples
    $('#classic-examples-dialog').modal();
    $('#classic-examples-dialog a').click(function(ev) {
	ev.preventDefault();
	load_example(ev.target.id);
    });

    // Source code input field
    $('#sourcecode1')
    //.keydown(handle_keydown)
    .linedtextarea()
    .attr('spellcheck', false);


    //$('#dialog-choose-function').dialog({ autoOpen: false, width: 400 });

    // Main tabs
    $('#result_tab_link').click(function(ev) {
	ev.preventDefault();
	if (performed == false)
	    performed = perform_operation(1, 'result');
    });
    $('#graph_tab_link').click(function(ev) {
	ev.preventDefault();
	if (created_graph == false)
	    created_graph = create_graph(ui.index, graph_id);
    });

    // Run button
    $("#run-button").click(function(ev) {
	ev.preventDefault();
	activate_tab('result_tab_link');
	if (performed == false)
	    performed = perform_operation(1, 'result');
    });

    /*

    // Save/print buttons
    $("input:submit", ".save_results").button();
    $("input:submit", ".print_results").button().click(function(event) {
        var content = document.getElementById("resultcode");
        var printFrame = document.getElementById("iframetoprint").contentWindow;
        printFrame.document.ope();
        printFrame.document.write(content.innerHTML);
        printFrame.document.close();
        printFrame.focus();
        printFrame.print();
    });
    deactivate_buttons();

    $('#resizing_source input:submit').button();

*/
});

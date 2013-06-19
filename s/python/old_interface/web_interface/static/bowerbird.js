/*
* javascript methods
*/

// define the remove row callback
function removeRow(event) {
    // stop default click handling
    event.preventDefault();
    // remove the row clicked on
    $(this).parents("tr.schedule").remove();
}


jQuery.validator.addMethod("unique_title", function(value, element) {
        var count = 0;
        $("input.schedule_title").each( function() { if ($(this).val() == value) { count++; } });
        return count < 2;
    }, "Please enter a unique title.");


// on document load
function onReadySchedule() {
    // add handler to remove button
    $("input.schedule_remove").click(removeRow);

    // add handler to "Add Entry" button
    $("input.schedule_add").click(function(event) {
        // stop default click handling
        event.preventDefault();
        // find the a new unique id for the new row (one more than the previous row)
        var new_id = 0;
        $(this).parents("tr").prev("tr.schedule").each(function() {
            var id = Number($(this).attr('id'));
            if (id >= new_id) {
                new_id = id + 1;
            }
        });
        // add a new row before it
        $(this).parents("tr").before('<tr id="'+new_id+'" class="schedule">'
            + '<td class="schedule">'
            + '<input name="'+new_id+'.title" class="schedule_title required unique_title" minlength="3" type="text" value="" />'
            + '</td><td class="schedule">'
			+ '<input name="'+new_id+'.start" class="schedule_time required mytime" type="text" value="" />'
            + '</td><td class="schedule">'
			+ '<input name="'+new_id+'.finish" class="schedule_time required mytime" type="text" value="" />'
            + '</td><td class="schedule_remove">'
            + '<input name="'+new_id+'.remove" class="schedule_remove" type="submit" value="Remove" />'
            + '</td></tr>');

        // add handler to remove button
        $(this).parents("tr").prev("tr.schedule").find("input.schedule_remove").click(removeRow);

        return false;
    });

    // connect the form validation
    $("#scheduleForm").validate();
}

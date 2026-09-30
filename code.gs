/** GroupSync backend. Deploy as a web app, executing as the owner. */
function setup() {
  const p = PropertiesService.getScriptProperties();
  if (!p.getProperty('API_KEY')) {
    p.setProperty('API_KEY', Utilities.getUuid() + Utilities.getUuid());
  }
  console.log('API key (keep private): ' + p.getProperty('API_KEY'));
}

function doPost(e) {
  try {
    const body = JSON.parse(e.postData.contents);
    const p = PropertiesService.getScriptProperties();
    const key = p.getProperty('API_KEY');
    if (!key || body.apiKey !== key) throw new Error('Unauthorized request.');
    let result;
    if (body.action === 'Ping') result = {message: 'GroupSync connected'};
    else if (body.action === 'Create') result = createEvent_(body.event);
    else if (body.action === 'Collect') result = collect_(body);
    else if (body.action === 'Cancel') result = cancelEvent_(body);
    else if (body.action === 'ConvertTimeZone') {
  result = convertToEventTime_(
    body.date,
    body.time,
    body.participantTimeZone,
    body.eventTimeZone
  );
}
    else throw new Error('Unknown action.');
    return json_(Object.assign({ok: true}, result));
  } catch (err) {
    return json_({ok: false, error: String(err.message || err)});
  }
}
function json_(value) {
  return ContentService.createTextOutput(JSON.stringify(value))
    .setMimeType(ContentService.MimeType.JSON);
}
function validateEvent_(event) {
  if (!event || typeof event.eventName !== 'string' ||
      !event.eventName.trim() || event.eventName.length > 99)
    throw new Error('Invalid event name.');
  if (!Number.isInteger(event.durationMinutes) || event.durationMinutes < 1 ||
      event.durationMinutes > 1440) throw new Error('Invalid meeting duration.');
  if (!Array.isArray(event.dates) || event.dates.length < 1 || event.dates.length > 14)
    throw new Error('Provide 1–14 dates.');
  event.dates.forEach(d => {
    if (typeof d.label !== 'string' || !d.label.trim() || d.label.length > 31 ||
        !Number.isInteger(d.startMinutes) || !Number.isInteger(d.endMinutes) ||
        d.startMinutes < 0 || d.endMinutes > 1440 || d.endMinutes <= d.startMinutes ||
        d.startMinutes % 30 || d.endMinutes % 30 ||
        d.blockCount !== (d.endMinutes - d.startMinutes) / 30)
      throw new Error('Invalid date range.');
  });
}
function time_(minutes) {
  if (minutes === 1440) return '00:00 (next day)';
  return String(Math.floor(minutes / 60)).padStart(2, '0') + ':' +
    String(minutes % 60).padStart(2, '0');
}
function createEvent_(event) {
  validateEvent_(event);
  const form = FormApp.create('GroupSync: ' + event.eventName);
  form.setDescription('Meeting duration: ' + event.durationMinutes +
    ' minutes. All times use the organizer’s local time zone. Select one answer per row.')
    .setConfirmationMessage('Your availability has been saved.')
    .setCollectEmail(false).setLimitOneResponsePerUser(false);
  const nameId = form.addTextItem().setTitle('Your name').setRequired(true).getId();
  const timeZoneMetadata = addTimeZoneAvailability_(form, event);
  const sheet = SpreadsheetApp.create('GroupSync responses: ' + event.eventName);
  form.setDestination(FormApp.DestinationType.SPREADSHEET, sheet.getId());
  if (form.supportsAdvancedResponderPermissions()) form.setPublished(true);
  form.setAcceptingResponses(true);
  const record = Object.assign({
  event: event,
  nameId: nameId,
  sheetId: sheet.getId()
}, timeZoneMetadata);
  // Store metadata in a private sheet tab, avoiding Script Properties per-value limits.
  const metadata = sheet.insertSheet('_GroupSync');
  metadata.getRange('A1').setValue(JSON.stringify(record));
  metadata.hideSheet();
  PropertiesService.getScriptProperties().setProperty('FORM_' + form.getId(), sheet.getId());
  return {formId: form.getId(), responderUrl: form.getPublishedUrl(), sheetUrl: sheet.getUrl()};
}
function collect_(body) {
  if (
    typeof body.formId !== 'string' ||
    !/^[A-Za-z0-9_-]+$/.test(body.formId)
  ) {
    throw new Error('Invalid form ID.');
  }

  const sheetId = PropertiesService.getScriptProperties()
    .getProperty('FORM_' + body.formId);

  if (!sheetId) {
    throw new Error('This deployment did not create that form.');
  }

  const expected = body.expectedResponses;

  if (!Number.isInteger(expected) || expected < 1 || expected > 50) {
    throw new Error('Expected responses must be 1–50.');
  }

  const record = JSON.parse(
    SpreadsheetApp.openById(sheetId)
      .getSheetByName('_GroupSync')
      .getRange('A1')
      .getValue()
  );

  const form = FormApp.openById(body.formId);
  const responses = form.getResponses();
  const canceled = record.status === 'canceled';
  const ready = responses.length >= expected;

  const lines = responses.slice(0, expected).map(response =>
    timeZoneResponseToTsv_(response, record)
  );

  if (ready && !canceled && body.closeWhenReady === true) {
    form.setAcceptingResponses(false);
  }

  return {
    responseCount: responses.length,
    ready: ready,
    canceled: canceled,
    status: canceled ? 'canceled' : 'active',
    tsv: lines.length ? lines.join('\n') + '\n' : ''
  };
}
   
function logGroupSyncTestFormId() {
  const sheetId = '1covcgxIA4r6GL8YgDjQpdmK7UbvceS5gRc9laD8lv2w';
  const properties = PropertiesService.getScriptProperties().getProperties();
  const key = Object.keys(properties).find(
    name => name.startsWith('FORM_') && properties[name] === sheetId
  );
  if (!key) throw new Error('No GroupSync form found for that response sheet.');
  console.log(key.substring(5));
}
function cancelEvent_(body) {
  if (
    typeof body.formId !== 'string' ||
    !/^[A-Za-z0-9_-]+$/.test(body.formId)
  ) {
    throw new Error('Invalid form ID.');
  }

  // Only allow forms created by this GroupSync deployment.
  const sheetId = PropertiesService.getScriptProperties()
    .getProperty('FORM_' + body.formId);

  if (!sheetId) {
    throw new Error('This deployment did not create that form.');
  }

  // Close the form while preserving existing responses.
  const form = FormApp.openById(body.formId);
  form.setAcceptingResponses(false);

  // Record cancellation in the existing event metadata.
  const cell = SpreadsheetApp.openById(sheetId)
    .getSheetByName('_GroupSync')
    .getRange('A1');

  const record = JSON.parse(cell.getValue());
  record.status = 'canceled';
  record.canceledAt = record.canceledAt || new Date().toISOString();
  cell.setValue(JSON.stringify(record));

  return {
    formId: body.formId,
    status: 'canceled',
    acceptingResponses: false
  };
}
/**
 * Converts a participant's local date/time to the event time zone.
 *
 * date: "2026-10-03"
 * time: "15:00" (24-hour format)
 * participantZone: "America/Los_Angeles"
 * eventZone: "America/New_York"
 */
function convertToEventTime_(date, time, participantZone, eventZone) {
  // Validate the time-zone names.
  [participantZone, eventZone].forEach(zone => {
    if (typeof zone !== 'string' || !zone) {
      throw new Error('A time zone is required.');
    }
    new Intl.DateTimeFormat('en-US', {timeZone: zone});
  });

  const localText = date + ' ' + time;

  if (!/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}$/.test(localText)) {
    throw new Error('Use YYYY-MM-DD and HH:MM.');
  }

  // This is a reference value, not yet the actual UTC instant.
  const reference = new Date(date + 'T' + time + ':00Z');

  if (
    isNaN(reference.getTime()) ||
    Utilities.formatDate(reference, 'UTC', 'yyyy-MM-dd HH:mm') !== localText
  ) {
    throw new Error('Invalid date or time.');
  }

  // Find nearby offsets to account for daylight saving transitions.
  const offsets = new Set();

  for (let hours = -48; hours <= 48; hours += 6) {
    const probe = new Date(reference.getTime() + hours * 3600000);
    const offset = Utilities.formatDate(probe, participantZone, 'Z');
    const sign = offset[0] === '-' ? -1 : 1;
    const minutes = Number(offset.slice(1, 3)) * 60 +
                    Number(offset.slice(3, 5));

    offsets.add(sign * minutes);
  }

  const matches = [];

  offsets.forEach(offsetMinutes => {
    const instant = new Date(
      reference.getTime() - offsetMinutes * 60000
    );

    if (
      Utilities.formatDate(
        instant, participantZone, 'yyyy-MM-dd HH:mm'
      ) === localText
    ) {
      matches.push(instant);
    }
  });

  if (matches.length === 0) {
    throw new Error(
      'This local time does not exist because the clocks move forward.'
    );
  }

  if (matches.length > 1) {
    throw new Error(
      'This local time occurs twice because the clocks move back. ' +
      'Choose an unambiguous time.'
    );
  }

  const instant = matches[0];
  const eventTime = Utilities.formatDate(instant, eventZone, 'HH:mm');
  const parts = eventTime.split(':').map(Number);

  return {
    eventDate: Utilities.formatDate(instant, eventZone, 'yyyy-MM-dd'),
    eventTime: eventTime,
    eventMinutes: parts[0] * 60 + parts[1],
    eventTimeZone: eventZone,
    utc: instant.toISOString()
  };
}
/**
 * Adds a required time-zone question and local availability sections.
 *
 * Call on a new form AFTER adding "Your name".
 * Do not also add your old availability grids.
 *
 * event.timeZone must be an IANA zone such as America/New_York.
 * Date labels must be YYYY-MM-DD.
 */
function addTimeZoneAvailability_(form, event) {
  const coordinatorZone = event.timeZone;

  if (!coordinatorZone) {
    throw new Error('Event timeZone is required.');
  }

  const zones = [...new Set([
    coordinatorZone,
    'America/New_York',
    'America/Chicago',
    'America/Denver',
    'America/Phoenix',
    'America/Los_Angeles',
    'America/Anchorage',
    'Pacific/Honolulu',
    'Europe/London',
    'Europe/Paris',
    'Asia/Kolkata',
    'Asia/Tokyo',
    'Australia/Sydney'
  ])];

  // Resolve event blocks to actual instants once.
  const blocks = event.dates.map(date => {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(date.label)) {
      throw new Error('Time-zone support requires YYYY-MM-DD dates.');
    }

    return Array.from({length: date.blockCount}, (_, block) => {
      const minutes = date.startMinutes + block * 30;
      const time = String(Math.floor(minutes / 60)).padStart(2, '0') +
        ':' + String(minutes % 60).padStart(2, '0');

      const converted = convertToEventTime_(
        date.label, time, coordinatorZone, coordinatorZone
      );

      const start = new Date(converted.utc);
      const end = new Date(start.getTime() + 30 * 60000);

      return {start: start, end: end};
    });
  });

  const zoneQuestion = form.addMultipleChoiceItem()
    .setTitle('Your time zone')
    .setHelpText('Choose the zone where you will enter your availability.')
    .setRequired(true);

  const sections = [];

  zones.forEach(zone => {
    const page = form.addPageBreakItem()
      .setTitle('Availability — ' + zone);

    const gridIds = blocks.map((dateBlocks, dateIndex) => {
      const rows = dateBlocks.map(block => {
  const start = Utilities.formatDate(
    block.start, zone, 'MMM d, yyyy h:mm a'
  );
  const end = Utilities.formatDate(
    block.end, zone, 'MMM d, yyyy h:mm a'
  );

  return start + ' → ' + end;
});

      return form.addGridItem()
        .setTitle(
          'Local times for event date ' + event.dates[dateIndex].label
        )
        .setHelpText('Times below are in ' + zone + '.')
        .setRows(rows)
        .setColumns(['Available', 'Maybe', 'Unavailable'])
        .setRequired(true)
        .getId();
    });

    sections.push({zone: zone, page: page, gridIds: gridIds});
  });

  // In Forms, navigation on a page break controls the preceding section.
  // Each following boundary submits the preceding time-zone section.
  for (let i = 1; i < sections.length; i++) {
    sections[i].page.setGoToPage(FormApp.PageNavigationType.SUBMIT);
  }

  // The last section submits naturally at the end of the form.
  zoneQuestion.setChoices(
    sections.map(section =>
      zoneQuestion.createChoice(section.zone, section.page)
    )
  );

  // Save this mapping in the existing event metadata.
  return {
    timeZoneQuestionId: zoneQuestion.getId(),
    coordinatorTimeZone: coordinatorZone,
    zoneGrids: sections.map(section => ({
      zone: section.zone,
      gridIds: section.gridIds
    }))
  };
}


/**
 * Exports a response in the existing coordinator-ordered TSV format.
 * record includes event, nameId, and the metadata returned above.
 */
function timeZoneResponseToTsv_(response, record) {
  const answers = {};

  response.getItemResponses().forEach(itemResponse => {
    answers[String(itemResponse.getItem().getId())] =
      itemResponse.getResponse();
  });

  const zone = answers[String(record.timeZoneQuestionId)];
  const section = record.zoneGrids.find(item => item.zone === zone);

  if (!section) {
    throw new Error('Missing or unsupported participant time zone.');
  }

  const name = String(answers[String(record.nameId)] || '')
    .replace(/[\t\r\n]/g, ' ')
    .trim();

  if (!name) {
    throw new Error('Participant name is missing.');
  }

  const fields = [name];

  record.event.dates.forEach((date, dateIndex) => {
    const rows = answers[String(section.gridIds[dateIndex])];

    if (!Array.isArray(rows) || rows.length !== date.blockCount) {
      throw new Error('Participant availability is incomplete.');
    }

    rows.forEach(answer => {
      const code = {
        Available: 'A',
        Maybe: 'M',
        Unavailable: 'U'
      }[answer];

      if (!code) {
        throw new Error('Invalid availability answer.');
      }

      fields.push(code);
    });
  });

  return fields.join('\t');
}


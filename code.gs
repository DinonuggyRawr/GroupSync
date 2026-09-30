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
  const gridIds = event.dates.map(d => {
    const rows = Array.from({length: d.blockCount}, (_, i) =>
      time_(d.startMinutes + i * 30) + ' - ' + time_(d.startMinutes + (i + 1) * 30));
    return form.addGridItem().setTitle(d.label).setRows(rows)
      .setColumns(['Available', 'Maybe', 'Unavailable']).setRequired(true).getId();
  });
  const sheet = SpreadsheetApp.create('GroupSync responses: ' + event.eventName);
  form.setDestination(FormApp.DestinationType.SPREADSHEET, sheet.getId());
  if (form.supportsAdvancedResponderPermissions()) form.setPublished(true);
  form.setAcceptingResponses(true);
  const record = {event: event, nameId: nameId, gridIds: gridIds, sheetId: sheet.getId()};
  // Store metadata in a private sheet tab, avoiding Script Properties per-value limits.
  const metadata = sheet.insertSheet('_GroupSync');
  metadata.getRange('A1').setValue(JSON.stringify(record));
  metadata.hideSheet();
  PropertiesService.getScriptProperties().setProperty('FORM_' + form.getId(), sheet.getId());
  return {formId: form.getId(), responderUrl: form.getPublishedUrl(), sheetUrl: sheet.getUrl()};
}
function collect_(body) {
  if (typeof body.formId !== 'string' || !/^[A-Za-z0-9_-]+$/.test(body.formId))
    throw new Error('Invalid form ID.');
  const sheetId = PropertiesService.getScriptProperties().getProperty('FORM_' + body.formId);
  if (!sheetId) throw new Error('This deployment did not create that form.');
  const expected = body.expectedResponses;
  if (!Number.isInteger(expected) || expected < 1 || expected > 50)
    throw new Error('Expected responses must be 1–50.');
  const record = JSON.parse(SpreadsheetApp.openById(sheetId)
    .getSheetByName('_GroupSync').getRange('A1').getValue());
  const form = FormApp.openById(body.formId);
  const responses = form.getResponses();
  const ready = responses.length >= expected;
  // One submission counts as one participant, matching the current desktop program.
  const lines = responses.slice(0, expected).map(response => {
    const answers = {};
    response.getItemResponses().forEach(r => answers[String(r.getItem().getId())] = r.getResponse());
    const name = String(answers[String(record.nameId)] || 'Unnamed')
      .replace(/[\t\r\n]/g, ' ').trim() || 'Unnamed';
    const fields = [name];
    record.event.dates.forEach((d, i) => {
      const rows = answers[String(record.gridIds[i])] || [];
      for (let b = 0; b < d.blockCount; b++) {
        fields.push(rows[b] === 'Available' ? 'A' : rows[b] === 'Maybe' ? 'M' : 'U');
      }
    });
    return fields.join('\t');
  });
  if (ready && body.closeWhenReady === true) form.setAcceptingResponses(false);
  return {responseCount: responses.length, ready: ready,
    tsv: lines.length ? lines.join('\n') + '\n' : ''};
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

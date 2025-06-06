async function fetchMarkdown() {
  const response = await fetch('README.md');
  const text = await response.text();
  const html = markdownToHtml(text);
  document.getElementById('markdown').innerHTML = html;
}

function markdownToHtml(md) {
  const lines = md.split('\n');
  const html = lines.map(line => {
    if (/^# (.*)/.test(line)) return `<h1>${RegExp.$1}</h1>`;
    if (/^## (.*)/.test(line)) return `<h2>${RegExp.$1}</h2>`;
    if (/^### (.*)/.test(line)) return `<h3>${RegExp.$1}</h3>`;
    if (/^- (.*)/.test(line)) return `<li>${RegExp.$1}</li>`;
    if (/^\d+\. (.*)/.test(line)) return `<li>${RegExp.$1}</li>`;
    if (/^\*\*(.+)\*\*/.test(line)) return `<p><strong>${RegExp.$1}</strong></p>`;
    if (/^\*(.+)\*/.test(line)) return `<p><em>${RegExp.$1}</em></p>`;
    if (/^```/.test(line)) return `<pre><code>`;
    if (/^```$/.test(line)) return `</code></pre>`;
    if (line.trim() === '') return '';
    return `<p>${line}</p>`;
  });

  // Wrap <li> inside <ul> or <ol>
  return wrapListItems(html).join('\n');
}

function wrapListItems(lines) {
  const result = [];
  let inUl = false, inOl = false;

  for (let line of lines) {
    if (line.startsWith('<li>')) {
      if (!inUl && /^\d+\. /.test(line)) {
        result.push('<ol>');
        inOl = true;
      } else if (!inUl && /^- /.test(line)) {
        result.push('<ul>');
        inUl = true;
      }
      result.push(line);
    } else {
      if (inUl) {
        result.push('</ul>');
        inUl = false;
      }
      if (inOl) {
        result.push('</ol>');
        inOl = false;
      }
      result.push(line);
    }
  }

  if (inUl) result.push('</ul>');
  if (inOl) result.push('</ol>');

  return result;
}

fetchMarkdown();

.pragma library

function localPath(url) {
    return decodeURIComponent(url.toString().replace(/^file:\/\//, ""))
}
